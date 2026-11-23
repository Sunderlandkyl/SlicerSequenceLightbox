"""The Sequence Lightbox module in the web application of Slicer (SlicerWeb).

SlicerWeb builds the parts of the module that do not use Qt - the view node, the logic and the
displayable manager - and none of its Qt classes. What those classes do is done here instead, with
the API that a scripted module has in the browser:

- the lightbox view is registered with the layout manager (qMRMLLayoutSequenceLightboxViewFactory),
- the layouts are added to the layout node and to the layout menu, and the new lightbox views are
  given their default settings (qSlicerSequenceLightboxModule),
- the panel of the module sets up the lightbox (qSlicerSequenceLightboxModuleWidget and the
  controller bar of the view, qMRMLSequenceLightboxViewControllerWidget).

This module is only built for SlicerWeb (see SequenceLightbox/CMakeLists.txt).
"""

import logging

import ctk
import qt
import vtk

import slicer
from slicer.ScriptedLoadableModule import ScriptedLoadableModule, ScriptedLoadableModuleWidget
from slicer.util import VTKObservationMixin

VIEW_NODE_CLASS = "vtkMRMLSequenceLightboxViewNode"
DISPLAYABLE_MANAGER_CLASS = "vtkMRMLSequenceLightboxDisplayableManager"

# The layouts, as in qSlicerSequenceLightboxModule: (offset from SlicerLayoutUserView, name in the
# layout menu, name in the panel, singleton tag of the lightbox view, 3D, timeline, lightbox only)
LAYOUTS = [
    (601, "Sequence lightbox (slice)", "Lightbox next to the slice view", "SequenceLightbox1", False, False, False),
    (602, "Sequence lightbox (3D)", "Lightbox next to the 3D view", "SequenceLightbox3D", True, False, False),
    (603, "Sequence timeline (slice)", "Timeline below the slice view", "SequenceTimeline1", False, True, False),
    (604, "Sequence timeline (3D)", "Timeline below the 3D view", "SequenceTimeline3D", True, True, False),
    (605, "Sequence lightbox only (slice)", "Lightbox only (slice)", "SequenceLightbox1", False, False, True),
    (606, "Sequence lightbox only (3D)", "Lightbox only (3D)", "SequenceLightbox3D", True, False, True),
]

RED_VIEW = ('<view class="vtkMRMLSliceNode" singletontag="Red">'
            '<property name="viewlabel" action="default">R</property>'
            '<property name="orientation" action="default">Axial</property>'
            '<property name="viewcolor" action="default">#F34A33</property></view>')
THREED_VIEW = '<view class="vtkMRMLViewNode" singletontag="1"><property name="viewlabel" action="default">1</property></view>'

# qMRMLSequenceLightboxViewControllerWidget lists the modes in this order
ITEM_SELECTION_MODES = [
    ("Consecutive items from a fixed item", 0),
    ("Consecutive items from the selected item", 1),
    ("Consecutive items around the selected item", 3),
    ("Custom list of items", 2),
]
MODE_FIXED, MODE_FOLLOW_SELECTED, MODE_CUSTOM, MODE_CENTER_SELECTED = 0, 1, 2, 3


def layoutId(offset):
    return slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + offset


def viewDescription(className, singletonTag, label):
    return f'<view class="{className}" singletontag="{singletonTag}"><property name="viewlabel" action="default">{label}</property></view>'


def isTimelineLayoutName(layoutName):
    return bool(layoutName) and layoutName.startswith("SequenceTimeline")


def numberOfListedItems(text):
    """How many items a list such as "0, 3, 7-9" names."""
    count = 0
    for part in (text or "").split(","):
        bounds = [b.strip() for b in part.split("-") if b.strip()]
        try:
            count += abs(int(bounds[-1]) - int(bounds[0])) + 1 if bounds else 0
        except ValueError:
            pass
    return count


def lightboxViewNodes(scene):
    nodes = scene.GetNodesByClass(VIEW_NODE_CLASS)
    nodes.UnRegister(None)
    return [nodes.GetItemAsObject(i) for i in range(nodes.GetNumberOfItems())]


def applyDefaultConfiguration(scene):
    """Give the lightbox views that have none a sequence browser and a view to follow."""
    for viewNode in lightboxViewNodes(scene):
        if viewNode.GetSequenceBrowserNode() is None:
            browser = scene.GetFirstNodeByClass("vtkMRMLSequenceBrowserNode")
            if browser is not None:
                viewNode.SetAndObserveSequenceBrowserNodeID(browser.GetID())
        if viewNode.GetReferenceViewNode() is None:
            reference = None
            if "3D" in (viewNode.GetLayoutName() or ""):
                reference = scene.GetSingletonNode("1", "vtkMRMLViewNode")
            if reference is None:
                reference = scene.GetSingletonNode("Red", "vtkMRMLSliceNode")
            if reference is not None:
                viewNode.SetAndObserveReferenceViewNodeID(reference.GetID())


def applyTimelineConfiguration(viewNode):
    """A strip of items around the selected one."""
    wasModifying = viewNode.StartModify()
    viewNode.SetRows(1)
    viewNode.SetColumns(7)
    viewNode.SetItemSelectionMode(MODE_CENTER_SELECTED)
    viewNode.SetHighlightSelectedItem(True)
    viewNode.EndModify(wasModifying)


class SequenceLightboxWebSupport(VTKObservationMixin):
    """What the Qt classes of the module do for the application, apart from their widgets."""

    def __init__(self):
        VTKObservationMixin.__init__(self)
        self.scene = None
        self.observedViewNodes = {}   # node ID -> (node, observer tag)
        self.layoutUpdateTimer = qt.QTimer()
        self.layoutUpdateTimer.setSingleShot(True)
        self.layoutUpdateTimer.setInterval(0)
        self.layoutUpdateTimer.connect("timeout()", self.updateLayoutDescriptions)
        self.companions = None

    @staticmethod
    def isSupported():
        """Whether the application can display a view of a module (SlicerWeb with view types)."""
        layoutManager = slicer.app.layoutManager()
        return layoutManager is not None and hasattr(layoutManager, "registerViewType")

    def setup(self):
        if not self.isSupported():
            logging.warning("Sequence Lightbox: this application cannot display the views of an extension; the lightbox view is not available")
            return False
        factory = slicer.vtkMRMLSequenceLightboxViewDisplayableManagerFactory.GetInstance()
        factory.SetMRMLApplicationLogic(slicer.app.applicationLogic())
        if not factory.IsDisplayableManagerRegistered(DISPLAYABLE_MANAGER_CLASS):
            factory.RegisterDisplayableManager(DISPLAYABLE_MANAGER_CLASS)
        slicer.app.layoutManager().registerViewType(VIEW_NODE_CLASS, factory, beforeRender=self.synchronizeTiles)

        self.scene = slicer.mrmlScene
        self.addObserver(self.scene, slicer.vtkMRMLScene.NodeAddedEvent, self.onNodeAdded)
        self.addObserver(self.scene, slicer.vtkMRMLScene.NodeRemovedEvent, self.onNodeRemoved)
        self.addObserver(self.scene, slicer.vtkMRMLScene.EndImportEvent, self.onEndImport)
        self.addObserver(self.scene, slicer.vtkMRMLScene.EndCloseEvent, self.onEndClose)
        for viewNode in lightboxViewNodes(self.scene):
            self.observeViewNode(viewNode)
        self.updateLayoutDescriptions()
        return True

    @staticmethod
    def synchronizeTiles(view):
        """The tiles show the items they have to before the view renders (qMRMLSequenceLightboxView::forceRender)."""
        group = view.GetDisplayableManagerGroup()
        displayableManager = group.GetDisplayableManagerByClassName(DISPLAYABLE_MANAGER_CLASS) if group else None
        if displayableManager is not None:
            displayableManager.SynchronizeTiles()

    # ------------------------------------------------------------------ layouts
    def companionView(self, lightboxTag, threeD):
        """The view shown next to the lightbox: the view that the lightbox follows."""
        default = THREED_VIEW if threeD else RED_VIEW
        defaultClass, defaultName = ("vtkMRMLViewNode", "1") if threeD else ("vtkMRMLSliceNode", "Red")
        viewNode = self.scene.GetSingletonNode(lightboxTag, VIEW_NODE_CLASS) if self.scene else None
        reference = viewNode.GetReferenceViewNode() if viewNode is not None else None
        if (reference is None or not reference.GetLayoutName() or reference.IsA(VIEW_NODE_CLASS)
                or (reference.GetClassName() == defaultClass and reference.GetLayoutName() == defaultName)):
            return default
        return viewDescription(reference.GetClassName(), reference.GetLayoutName(), reference.GetLayoutLabel() or reference.GetLayoutName())

    def layoutDescriptions(self):
        descriptions = {}
        for offset, name, _title, tag, threeD, timeline, only in LAYOUTS:
            lightbox = viewDescription(VIEW_NODE_CLASS, tag, "T" if timeline else "L")
            if only:
                xml = f'<layout type="horizontal"><item>{lightbox}</item></layout>'
            else:
                companion = self.companionView(tag, threeD)
                direction, first, second = ("vertical", 700, 300) if timeline else ("horizontal", 400, 600)
                xml = (f'<layout type="{direction}" split="true"><item splitSize="{first}">{companion}</item>'
                       f'<item splitSize="{second}">{lightbox}</item></layout>')
            descriptions[offset] = (name, xml)
        return descriptions

    def updateLayoutDescriptions(self):
        if self.scene is None or self.scene.IsImporting() or self.scene.IsRestoring() or self.scene.IsClosing():
            return
        layoutManager = slicer.app.layoutManager()
        for offset, (name, xml) in self.layoutDescriptions().items():
            layoutManager.addLayout(layoutId(offset), xml, name)

    def requestLayoutUpdate(self):
        self.layoutUpdateTimer.start()

    # ------------------------------------------------------------------ scene
    def observeViewNode(self, viewNode):
        if viewNode.GetID() in self.observedViewNodes:
            return
        tag = viewNode.AddObserver(viewNode.ReferenceViewNodeModifiedEvent, self.onReferenceViewModified)
        self.observedViewNodes[viewNode.GetID()] = (viewNode, tag)

    def onReferenceViewModified(self, caller=None, event=None):
        # Invoked for every change of the view that is followed (each slice that is scrolled to):
        # the layouts only change with the view itself
        companions = tuple(self.companionView(tag, threeD) for _o, _n, _t, tag, threeD, _tl, only in LAYOUTS if not only)
        if companions != self.companions:
            self.companions = companions
            self.requestLayoutUpdate()

    @vtk.calldata_type(vtk.VTK_OBJECT)
    def onNodeAdded(self, caller, event, node):
        if node is None:
            return
        loading = self.scene.IsImporting() or self.scene.IsRestoring()
        if node.IsA("vtkMRMLSequenceBrowserNode"):
            if not loading:
                applyDefaultConfiguration(self.scene)
        elif node.IsA(VIEW_NODE_CLASS):
            self.observeViewNode(node)
            # A view that is made for a layout; one that is read from a file keeps its settings
            if not loading:
                applyDefaultConfiguration(self.scene)
                if isTimelineLayoutName(node.GetLayoutName()):
                    applyTimelineConfiguration(node)
            self.requestLayoutUpdate()

    @vtk.calldata_type(vtk.VTK_OBJECT)
    def onNodeRemoved(self, caller, event, node):
        if node is None or not node.IsA(VIEW_NODE_CLASS):
            return
        observed = self.observedViewNodes.pop(node.GetID(), None)
        if observed is not None:
            observed[0].RemoveObserver(observed[1])
        self.requestLayoutUpdate()

    def onEndImport(self, caller=None, event=None):
        applyDefaultConfiguration(self.scene)
        self.requestLayoutUpdate()

    def onEndClose(self, caller=None, event=None):
        applyDefaultConfiguration(self.scene)
        for viewNode in lightboxViewNodes(self.scene):
            if isTimelineLayoutName(viewNode.GetLayoutName()):
                applyTimelineConfiguration(viewNode)
        self.requestLayoutUpdate()


class SequenceLightboxWeb(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Sequence Lightbox"
        self.parent.categories = ["Sequences"]
        self.parent.dependencies = ["SequenceLightbox"]
        self.parent.contributors = ["Kyle Sunderland (Radixlab Medical Inc.)"]
        self.parent.helpText = ("Display several items of a sequence at once: a grid of sequence items (lightbox) or a strip of "
                                "the items around the current item (timeline), following a slice view or a 3D view.")
        self.parent.acknowledgementText = ""
        self.support = None
        # The module itself (its logic, without its Qt classes) has no panel in this application:
        # this module is the one to open
        moduleManager = slicer.app.moduleManager()
        loadableModule = moduleManager.module("SequenceLightbox") if hasattr(moduleManager, "module") else None
        if loadableModule is not None and hasattr(loadableModule, "hidden"):
            loadableModule.hidden = True
        slicer.app.connect("startupCompleted()", self.onStartupCompleted)

    def onStartupCompleted(self):
        if self.support is not None:
            return
        self.support = SequenceLightboxWebSupport()
        try:
            self.support.setup()
        except Exception:
            logging.exception("Sequence Lightbox: the lightbox view could not be set up")


class SequenceLightboxWebWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
    """The panel of the module: what the module panel and the controller bar of the view offer on the desktop."""

    def __init__(self, parent=None):
        ScriptedLoadableModuleWidget.__init__(self, parent)
        VTKObservationMixin.__init__(self)
        self.viewNode = None
        self.updatingGUI = False

    def setup(self):
        ScriptedLoadableModuleWidget.setup(self)

        # Lightbox
        lightboxButton = ctk.ctkCollapsibleButton()
        lightboxButton.text = "Lightbox"
        self.layout.addWidget(lightboxButton)
        form = qt.QFormLayout(lightboxButton)

        self.browserSelector = slicer.qMRMLNodeComboBox()
        self.browserSelector.nodeTypes = ["vtkMRMLSequenceBrowserNode"]
        self.browserSelector.noneEnabled = True
        self.browserSelector.addEnabled = False
        self.browserSelector.removeEnabled = False
        self.browserSelector.renameEnabled = False
        self.browserSelector.setMRMLScene(slicer.mrmlScene)
        self.browserSelector.setToolTip("The sequence browser whose items are displayed.")
        form.addRow("Sequence browser:", self.browserSelector)

        self.layoutSelector = qt.QComboBox()
        for offset, _name, title, _tag, _threeD, _timeline, _only in LAYOUTS:
            self.layoutSelector.addItem(title, offset)
        self.showButton = qt.QPushButton("Show")
        self.showButton.setToolTip("Show the lightbox in the chosen layout.")
        layoutRow = qt.QHBoxLayout()
        layoutRow.addWidget(self.layoutSelector)
        layoutRow.addWidget(self.showButton)
        form.addRow("Layout:", layoutRow)

        self.referenceViewSelector = slicer.qMRMLNodeComboBox()
        self.referenceViewSelector.nodeTypes = ["vtkMRMLSliceNode", "vtkMRMLViewNode"]
        self.referenceViewSelector.showHidden = True
        self.referenceViewSelector.noneEnabled = True
        self.referenceViewSelector.addEnabled = False
        self.referenceViewSelector.removeEnabled = False
        self.referenceViewSelector.renameEnabled = False
        self.referenceViewSelector.setMRMLScene(slicer.mrmlScene)
        self.referenceViewSelector.setToolTip("The view that the tiles follow: each tile shows what this view shows, for another item.")
        form.addRow("View:", self.referenceViewSelector)

        self.rowsSpinBox = qt.QSpinBox()
        self.rowsSpinBox.setRange(1, 16)
        form.addRow("Rows:", self.rowsSpinBox)
        self.columnsSpinBox = qt.QSpinBox()
        self.columnsSpinBox.setRange(1, 16)
        form.addRow("Columns:", self.columnsSpinBox)
        self.showAllItemsButton = qt.QPushButton("Show all items")
        self.showAllItemsButton.setToolTip("Choose the rows and columns so that every item is displayed, with tiles as large as the view allows.")
        form.addRow("Grid size:", self.showAllItemsButton)

        # Displayed items and the look of the tiles (the popup of the controller bar on the desktop)
        itemsButton = ctk.ctkCollapsibleButton()
        itemsButton.text = "Displayed items"
        self.layout.addWidget(itemsButton)
        itemsForm = qt.QFormLayout(itemsButton)

        self.modeSelector = qt.QComboBox()
        for text, mode in ITEM_SELECTION_MODES:
            self.modeSelector.addItem(text, mode)
        itemsForm.addRow("Displayed items:", self.modeSelector)
        self.itemStepSpinBox = qt.QSpinBox()
        self.itemStepSpinBox.setRange(1, 1000)
        itemsForm.addRow("Item step:", self.itemStepSpinBox)
        self.startItemSpinBox = qt.QSpinBox()
        self.startItemSpinBox.setRange(0, 100000)
        itemsForm.addRow("First item:", self.startItemSpinBox)
        self.customItemsEdit = qt.QLineEdit()
        self.customItemsEdit.setPlaceholderText("for example: 0, 3, 7-9")
        itemsForm.addRow("Items:", self.customItemsEdit)
        self.tileSpacingSpinBox = qt.QSpinBox()
        self.tileSpacingSpinBox.setRange(0, 50)
        self.tileSpacingSpinBox.setSuffix(" px")
        itemsForm.addRow("Tile spacing:", self.tileSpacingSpinBox)
        self.showLabelsCheckBox = qt.QCheckBox()
        itemsForm.addRow("Show item labels:", self.showLabelsCheckBox)
        self.highlightCheckBox = qt.QCheckBox()
        itemsForm.addRow("Highlight selected item:", self.highlightCheckBox)
        self.interactiveCheckBox = qt.QCheckBox()
        itemsForm.addRow("Interactive tiles:", self.interactiveCheckBox)

        self.layout.addStretch(1)

        self.browserSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onBrowserChanged)
        self.referenceViewSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onReferenceViewChanged)
        self.layoutSelector.connect("currentIndexChanged(int)", lambda index: self.updateViewNode())
        self.showButton.connect("clicked()", self.onShow)
        self.showAllItemsButton.connect("clicked()", self.onShowAllItems)
        self.rowsSpinBox.connect("valueChanged(int)", lambda value: self.setProperty("SetRows", int(value)))
        self.columnsSpinBox.connect("valueChanged(int)", lambda value: self.setProperty("SetColumns", int(value)))
        self.modeSelector.connect("currentIndexChanged(int)", self.onModeChanged)
        self.itemStepSpinBox.connect("valueChanged(int)", lambda value: self.setProperty("SetItemStep", int(value)))
        self.startItemSpinBox.connect("valueChanged(int)", lambda value: self.setProperty("SetStartItemNumber", int(value)))
        self.customItemsEdit.connect("editingFinished()", self.onCustomItemsEdited)
        self.tileSpacingSpinBox.connect("valueChanged(int)", lambda value: self.setProperty("SetTileSpacing", int(value)))
        self.showLabelsCheckBox.connect("toggled(bool)", lambda on: self.setProperty("SetShowItemLabels", bool(on)))
        self.highlightCheckBox.connect("toggled(bool)", lambda on: self.setProperty("SetHighlightSelectedItem", bool(on)))
        self.interactiveCheckBox.connect("toggled(bool)", lambda on: self.setProperty("SetInteractiveTiles", bool(on)))

        self.addObserver(slicer.mrmlScene, slicer.vtkMRMLScene.NodeAddedEvent, self.onSceneChanged)
        self.addObserver(slicer.mrmlScene, slicer.vtkMRMLScene.NodeRemovedEvent, self.onSceneChanged)
        self.addObserver(slicer.mrmlScene, slicer.vtkMRMLScene.EndImportEvent, self.onSceneChanged)
        self.addObserver(slicer.mrmlScene, slicer.vtkMRMLScene.EndCloseEvent, self.onSceneChanged)
        slicer.app.layoutManager().connect("layoutChanged(int)", self.onLayoutChanged)
        self.updateViewNode()

    def cleanup(self):
        self.removeObservers()
        self.viewNode = None

    def enter(self):
        self.updateViewNode()

    # ------------------------------------------------------------------ the view the panel sets up
    def selectedLayout(self):
        index = self.layoutSelector.currentIndex
        index = index() if callable(index) else index
        return LAYOUTS[index] if 0 <= index < len(LAYOUTS) else LAYOUTS[0]

    def findViewNode(self):
        """The lightbox view that is displayed, or else that of the chosen layout."""
        nodes = lightboxViewNodes(slicer.mrmlScene)
        for node in nodes:
            if node.IsMappedInLayout():
                return node
        return slicer.mrmlScene.GetSingletonNode(self.selectedLayout()[3], VIEW_NODE_CLASS)

    def updateViewNode(self):
        viewNode = self.findViewNode()
        if viewNode is not self.viewNode:
            if self.viewNode is not None:
                self.removeObserver(self.viewNode, vtk.vtkCommand.ModifiedEvent, self.onViewNodeModified)
            self.viewNode = viewNode
            if viewNode is not None:
                self.addObserver(viewNode, vtk.vtkCommand.ModifiedEvent, self.onViewNodeModified)
        self.updateGUIFromMRML()

    def onSceneChanged(self, caller=None, event=None, callData=None):
        self.updateViewNode()

    def onLayoutChanged(self, layout=None):
        # The layout that is shown is the one the panel offers to show
        current = slicer.app.layoutManager().layout
        current = current() if callable(current) else current
        for index, entry in enumerate(LAYOUTS):
            if layoutId(entry[0]) == current:
                wasUpdating, self.updatingGUI = self.updatingGUI, True
                self.layoutSelector.setCurrentIndex(index)
                self.updatingGUI = wasUpdating
        self.updateViewNode()

    def onViewNodeModified(self, caller=None, event=None):
        self.updateGUIFromMRML()

    def updateGUIFromMRML(self):
        viewNode = self.viewNode
        self.updatingGUI = True
        try:
            for widget in (self.referenceViewSelector, self.rowsSpinBox, self.columnsSpinBox, self.showAllItemsButton, self.modeSelector,
                           self.itemStepSpinBox, self.startItemSpinBox, self.customItemsEdit, self.tileSpacingSpinBox,
                           self.showLabelsCheckBox, self.highlightCheckBox, self.interactiveCheckBox):
                widget.setEnabled(viewNode is not None)
            if viewNode is None:
                return
            browser = viewNode.GetSequenceBrowserNode()
            if browser is not None:
                self.browserSelector.setCurrentNode(browser)
            self.referenceViewSelector.setCurrentNode(viewNode.GetReferenceViewNode())
            self.rowsSpinBox.value = viewNode.GetRows()
            self.columnsSpinBox.value = viewNode.GetColumns()
            mode = viewNode.GetItemSelectionMode()
            self.modeSelector.setCurrentIndex([m for _text, m in ITEM_SELECTION_MODES].index(mode))
            self.itemStepSpinBox.value = viewNode.GetItemStep()
            self.itemStepSpinBox.setEnabled(mode != MODE_CUSTOM)
            self.startItemSpinBox.value = viewNode.GetStartItemNumber()
            self.startItemSpinBox.setEnabled(mode == MODE_FIXED)
            self.customItemsEdit.text = viewNode.GetCustomItemNumbersAsString()
            self.customItemsEdit.setEnabled(mode == MODE_CUSTOM)
            self.tileSpacingSpinBox.value = viewNode.GetTileSpacing()
            self.showLabelsCheckBox.checked = viewNode.GetShowItemLabels()
            self.highlightCheckBox.checked = viewNode.GetHighlightSelectedItem()
            self.interactiveCheckBox.checked = viewNode.GetInteractiveTiles()
        finally:
            self.updatingGUI = False

    # ------------------------------------------------------------------ from the panel to the view
    def setProperty(self, setter, value):
        if self.updatingGUI or self.viewNode is None:
            return
        getattr(self.viewNode, setter)(value)

    def onBrowserChanged(self, node=None):
        if self.updatingGUI or self.viewNode is None:
            return
        node = self.browserSelector.currentNode()
        self.viewNode.SetAndObserveSequenceBrowserNodeID(node.GetID() if node is not None else None)

    def onReferenceViewChanged(self, node=None):
        if self.updatingGUI or self.viewNode is None:
            return
        node = self.referenceViewSelector.currentNode()
        self.viewNode.SetAndObserveReferenceViewNodeID(node.GetID() if node is not None else None)

    def onModeChanged(self, index):
        if self.updatingGUI or self.viewNode is None or not 0 <= index < len(ITEM_SELECTION_MODES):
            return
        self.viewNode.SetItemSelectionMode(ITEM_SELECTION_MODES[index][1])

    def onCustomItemsEdited(self):
        if self.updatingGUI or self.viewNode is None:
            return
        text = self.customItemsEdit.text
        self.viewNode.SetCustomItemNumbersFromString(text() if callable(text) else text)

    def onShow(self):
        slicer.app.layoutManager().setLayout(layoutId(self.selectedLayout()[0]))
        self.updateViewNode()
        browser = self.browserSelector.currentNode()
        if self.viewNode is not None and browser is not None:
            self.viewNode.SetAndObserveSequenceBrowserNodeID(browser.GetID())

    def onShowAllItems(self):
        """As qMRMLSequenceLightboxWidget::showAllItems: the grid that shows every item with the largest tiles."""
        viewNode = self.viewNode
        if viewNode is None:
            return
        if viewNode.GetItemSelectionMode() == MODE_CUSTOM:
            numberOfItems = numberOfListedItems(viewNode.GetCustomItemNumbersAsString())
        else:
            browser = viewNode.GetSequenceBrowserNode()
            total = browser.GetNumberOfItems() if browser is not None else 0
            wasModifying = viewNode.StartModify()
            viewNode.SetItemSelectionMode(MODE_FIXED)
            viewNode.SetStartItemNumber(0)
            viewNode.EndModify(wasModifying)
            step = max(1, viewNode.GetItemStep())
            numberOfItems = (total + step - 1) // step
        width, height = 600, 400
        view = slicer.app.layoutManager().view(viewNode.GetLayoutName()) if hasattr(slicer.app.layoutManager(), "view") else None
        if view is not None and view.GetRenderWindow() is not None:
            width, height = view.GetRenderWindow().GetSize()
        if numberOfItems > 0:
            viewNode.FitGridToItems(numberOfItems, int(width), int(height))
