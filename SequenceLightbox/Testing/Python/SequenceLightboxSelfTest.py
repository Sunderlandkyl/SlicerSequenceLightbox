import time

import numpy as np

import qt
import vtk

import slicer
from slicer.ScriptedLoadableModule import *

#
# SequenceLightboxSelfTest
#


class SequenceLightboxSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "SequenceLightboxSelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["Sequences", "SequenceLightbox"]
        self.parent.contributors = ["Kyle Sunderland (Radixlab Medical Inc.)"]
        self.parent.helpText = """This is a self test of the sequence lightbox view."""
        parent.acknowledgementText = """This file was originally developed for the sequence lightbox view."""


#
# SequenceLightboxSelfTestWidget
#


class SequenceLightboxSelfTestWidget(ScriptedLoadableModuleWidget):
    def setup(self):
        ScriptedLoadableModuleWidget.setup(self)


#
# SequenceLightboxSelfTestLogic
#


class SequenceLightboxSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


#
# SequenceLightboxSelfTestTest
#


class SequenceLightboxSelfTestTest(ScriptedLoadableModuleTest):
    """Test of the sequence lightbox view.

    The lightbox view displays a grid of sequence items without adding anything to the scene,
    so the test checks both the rendered result and that the scene is not modified.
    """

    NUMBER_OF_ITEMS = 6
    INTENSITIES = [20, 60, 100, 140, 180, 220]
    SLICE_LIGHTBOX_LAYOUT = slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + 601
    THREED_LIGHTBOX_LAYOUT = slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + 602
    SLICE_TIMELINE_LAYOUT = slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + 603
    THREED_TIMELINE_LAYOUT = slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + 604
    SLICE_LIGHTBOX_ONLY_LAYOUT = slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + 605
    THREED_LIGHTBOX_ONLY_LAYOUT = slicer.vtkMRMLLayoutNode.SlicerLayoutUserView + 606

    def setUp(self):
        slicer.mrmlScene.Clear()
        # Control point labels are rendered with vtkLabelPlacementMapper, which (VTK 9.6) leaks its
        # label buckets on every frame in viewports whose origin is not zero, such as the tiles of
        # a lightbox (see addMarkupsSequence). Tiles of a previous test may still exist when a test
        # creates its markups nodes, so point labels are hidden in the default display nodes: a
        # display node created by CreateDefaultDisplayNodes() then never shows labels.
        for defaultDisplayNode in (slicer.vtkMRMLMarkupsFiducialDisplayNode(), slicer.vtkMRMLMarkupsROIDisplayNode(), slicer.vtkMRMLMarkupsDisplayNode()):
            defaultDisplayNode.SetPointLabelsVisibility(False)
            slicer.mrmlScene.AddDefaultNode(defaultDisplayNode)

    def runTest(self):
        self.setUp()
        self.test_SequenceLightbox()
        self.setUp()
        self.test_SequenceLightbox3D()
        self.setUp()
        self.test_SequenceLightboxInteraction()
        self.setUp()
        self.test_SequenceLightboxTileRenderers()
        self.setUp()
        self.test_SequenceLightboxSparseSequence()
        self.setUp()
        self.test_SequenceLightboxCompleteNodePlacement()
        self.setUp()
        self.test_SequenceLightboxPlaybackUpdates()
        self.setUp()
        self.test_SequenceLightboxNavigation()
        self.setUp()
        self.test_SequenceTimeline()
        self.setUp()
        self.test_SequenceLightboxReferenceViewLayout()
        self.setUp()
        self.test_SequenceLightboxOnly()
        self.setUp()
        self.test_SequenceLightboxSceneSaveLoad()

    def createSequenceBrowser(self):
        """Create a sequence of uniform volumes, each with a different intensity."""
        sequenceNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceNode", "TestSequence")
        sequenceNode.SetIndexName("frame")
        for itemIndex in range(self.NUMBER_OF_ITEMS):
            volumeNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLScalarVolumeNode", f"frame{itemIndex}")
            voxels = np.zeros((20, 30, 40), dtype=np.uint8)
            voxels[:] = self.INTENSITIES[itemIndex]
            slicer.util.updateVolumeFromArray(volumeNode, voxels)
            sequenceNode.SetDataNodeAtValue(volumeNode, str(itemIndex))
            slicer.mrmlScene.RemoveNode(volumeNode)

        browserNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceBrowserNode", "TestBrowser")
        browserNode.SetAndObserveMasterSequenceNodeID(sequenceNode.GetID())
        slicer.modules.sequences.logic().UpdateProxyNodesFromSequences(browserNode)
        proxyNode = browserNode.GetProxyNode(sequenceNode)

        # Fixed window/level, so that the rendered intensity of a tile only depends on the item.
        proxyNode.CreateDefaultDisplayNodes()
        proxyNode.GetDisplayNode().SetAutoWindowLevel(False)
        proxyNode.GetDisplayNode().SetWindowLevel(255.0, 127.5)
        return sequenceNode, browserNode, proxyNode

    def renderedImage(self, displayableManager):
        """Return the rendered content of the lightbox view."""
        import vtk

        renderWindow = displayableManager.GetTileRenderer(0).GetRenderWindow()
        # Render through the view widget: the tiles are synchronized with the scene right before
        # the view is rendered, which the view widget takes care of.
        for lightboxView in slicer.util.mainWindow().findChildren("qMRMLSequenceLightboxView"):
            if lightboxView.renderWindow() is renderWindow:
                lightboxView.forceRender()
        slicer.app.processEvents()
        windowToImage = vtk.vtkWindowToImageFilter()
        windowToImage.SetInput(renderWindow)
        windowToImage.SetInputBufferTypeToRGB()
        windowToImage.ReadFrontBufferOff()
        windowToImage.Update()
        return windowToImage.GetOutput()

    def tileCenterValues(self, image, rows, columns):
        """Return the rendered value at the center of each tile, top-left tile first."""
        dimensions = image.GetDimensions()
        values = []
        for row in range(rows):
            for column in range(columns):
                x = min(int((column + 0.5) * dimensions[0] / columns), dimensions[0] - 1)
                # Images have their origin at the bottom, tiles are numbered from the top.
                y = min(int((rows - 1 - row + 0.5) * dimensions[1] / rows), dimensions[1] - 1)
                values.append(int(image.GetScalarComponentAsFloat(x, y, 0, 0)))
        return values

    def test_SequenceLightbox(self):
        self.delayDisplay("Starting the sequence lightbox test")

        sequenceNode, browserNode, proxyNode = self.createSequenceBrowser()
        self.assertIsNotNone(proxyNode)
        self.assertEqual(browserNode.GetNumberOfItems(), self.NUMBER_OF_ITEMS)
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)

        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        self.assertEqual(layoutManager.layout, self.SLICE_LIGHTBOX_LAYOUT)

        self.delayDisplay("The lightbox view node is created with a usable default configuration")
        viewNode = slicer.mrmlScene.GetSingletonNode("SequenceLightbox1", "vtkMRMLSequenceLightboxViewNode")
        self.assertIsNotNone(viewNode)
        self.assertIsNotNone(viewNode.GetSequenceBrowserNode())
        self.assertIsNotNone(viewNode.GetReferenceViewNode())

        lightboxWidget = layoutManager.viewWidget(viewNode)
        self.assertIsNotNone(lightboxWidget)

        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemStep(1)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        slicer.app.processEvents()

        displayableManager = lightboxWidget.displayableManagerByClassName("vtkMRMLSequenceLightboxDisplayableManager")
        self.assertIsNotNone(displayableManager)
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)

        self.delayDisplay("Each tile displays a different sequence item")
        itemNumbers = [displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(6)]
        self.assertEqual(itemNumbers, list(range(6)))

        # The tile scene mirrors the main scene, sharing the image data of the sequence. The tile
        # that shows the selected item (item 0) displays the proxy node, exactly as the regular
        # views do, so it shares the image data of the proxy node instead.
        tileScene = displayableManager.GetTileScene(0)
        self.assertIsNotNone(tileScene)
        mirroredProxyNode = tileScene.GetNodeByID(proxyNode.GetID())
        self.assertIsNotNone(mirroredProxyNode)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        self.assertIs(mirroredProxyNode.GetImageData(), proxyNode.GetImageData())
        mirroredProxyNode1 = displayableManager.GetTileScene(1).GetNodeByID(proxyNode.GetID())
        self.assertIsNotNone(mirroredProxyNode1)
        self.assertIs(mirroredProxyNode1.GetImageData(), sequenceNode.GetNthDataNode(1).GetImageData())

        self.delayDisplay("The tiles render the items")
        renderedValues = self.tileCenterValues(self.renderedImage(displayableManager), 2, 3)
        self.assertEqual(renderedValues, self.INTENSITIES)

        self.delayDisplay("A custom list of items can be displayed")
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionCustom)
        viewNode.SetCustomItemNumbersFromString("5, 0, 3")
        slicer.app.processEvents()
        renderedValues = self.tileCenterValues(self.renderedImage(displayableManager), 2, 3)
        self.assertEqual(renderedValues[:3], [self.INTENSITIES[5], self.INTENSITIES[0], self.INTENSITIES[3]])
        # There are only three items in the list, so the remaining tiles are empty: they are not
        # drawn, the (gray) background of the view is visible in their place.
        backgroundValue = round(viewNode.GetBackgroundColor()[0] * 255.0)
        for renderedValue in renderedValues[3:]:
            self.assertAlmostEqual(renderedValue, backgroundValue, delta=1)
        self.assertEqual(viewNode.GetCustomItemNumbersAsString(), "5, 0, 3")

        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        slicer.app.processEvents()

        self.delayDisplay("The scene is not modified by the lightbox")
        # Only the proxy volume of the browser node is in the scene.
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLScalarVolumeNode"), 1)

        self.delayDisplay("The grid follows the selected item")
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFollowSelected)
        browserNode.SetSelectedItemNumber(2)
        slicer.app.processEvents()
        self.renderedImage(displayableManager)
        itemNumbers = [displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(6)]
        # Tiles that would show an item past the end of the sequence are empty.
        self.assertEqual(itemNumbers, [2, 3, 4, 5, -1, -1])

        self.delayDisplay("The tiles follow the reference view")
        redSliceNode = slicer.mrmlScene.GetSingletonNode("Red", "vtkMRMLSliceNode")
        redSliceNode.SetSliceOffset(redSliceNode.GetSliceOffset() + 2.0)
        slicer.app.processEvents()
        tileSliceNode = tileScene.GetSingletonNode("Red", "vtkMRMLSliceNode")
        self.assertIsNotNone(tileSliceNode)
        self.renderedImage(displayableManager)
        self.assertAlmostEqual(tileSliceNode.GetSliceOffset(), redSliceNode.GetSliceOffset(), places=3)

        self.delayDisplay("A node that is added to the scene appears in the tiles")
        addedModelNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLModelNode", "AddedWhileDisplayed")
        sphereSource = vtk.vtkSphereSource()
        sphereSource.SetRadius(30.0)
        sphereSource.Update()
        addedModelNode.SetAndObservePolyData(sphereSource.GetOutput())
        addedModelNode.CreateDefaultDisplayNodes()
        slicer.app.processEvents()
        self.renderedImage(displayableManager)
        addedModelNodeID = addedModelNode.GetID()
        mirroredModelNode = tileScene.GetNodeByID(addedModelNodeID)
        self.assertIsNotNone(mirroredModelNode)
        self.assertIs(mirroredModelNode.GetPolyData(), addedModelNode.GetPolyData())
        slicer.mrmlScene.RemoveNode(addedModelNode)
        slicer.app.processEvents()
        self.renderedImage(displayableManager)
        self.assertIsNone(tileScene.GetNodeByID(addedModelNodeID))

        self.delayDisplay("The 3D lightbox layout follows the 3D view")
        layoutManager.setLayout(self.THREED_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        threeDViewNode = slicer.mrmlScene.GetSingletonNode("SequenceLightbox3D", "vtkMRMLSequenceLightboxViewNode")
        self.assertIsNotNone(threeDViewNode)
        self.assertIsNotNone(threeDViewNode.GetReferenceViewNode())
        self.assertEqual(threeDViewNode.GetReferenceViewNode().GetClassName(), "vtkMRMLViewNode")
        threeDLightboxWidget = layoutManager.viewWidget(threeDViewNode)
        self.assertIsNotNone(threeDLightboxWidget)
        slicer.app.processEvents()
        threeDDisplayableManager = threeDLightboxWidget.displayableManagerByClassName("vtkMRMLSequenceLightboxDisplayableManager")
        self.assertIsNotNone(threeDDisplayableManager)
        self.assertEqual(threeDDisplayableManager.GetNumberOfTiles(), threeDViewNode.GetNumberOfTiles())

        self.delayDisplay("Closing the scene releases the tiles")
        slicer.mrmlScene.Clear()
        slicer.app.processEvents()

        self.delayDisplay("Sequence lightbox test passed")

    def addMarkupsSequence(self, browserNode):
        """Add a markups sequence to the browser: one control point per item, at a different location.

        The sequence index values match the ones of the master sequence of the browser (0, 1, ...).
        """
        markupsSequenceNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceNode", "TestMarkupsSequence")
        markupsSequenceNode.SetIndexName("frame")
        for itemIndex in range(self.NUMBER_OF_ITEMS):
            markupsNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", f"markups{itemIndex}")
            markupsNode.AddControlPoint(10.0 + 5.0 * itemIndex, 5.0, 10.0)
            markupsSequenceNode.SetDataNodeAtValue(markupsNode, str(itemIndex))
            slicer.mrmlScene.RemoveNode(markupsNode)
        browserNode.AddSynchronizedSequenceNodeID(markupsSequenceNode.GetID())
        slicer.modules.sequences.logic().UpdateProxyNodesFromSequences(browserNode)
        markupsProxyNode = browserNode.GetProxyNode(markupsSequenceNode)
        markupsProxyNode.CreateDefaultDisplayNodes()
        # vtkLabelPlacementMapper (VTK 9.6) leaks its label buckets on every frame in viewports
        # whose origin is larger than the bucket grid, which is the case for most tiles of a
        # lightbox. The leak is in VTK (RenderOverlay compares the grid size to the viewport origin
        # instead of the viewport size and does not delete the previous grid), it is not related to
        # the lightbox. Control point labels are hidden so that the leak check of the test passes.
        markupsProxyNode.GetDisplayNode().SetPointLabelsVisibility(False)
        return markupsSequenceNode, markupsProxyNode

    def lightboxView(self, layoutName):
        """Return the view node, the view widget and the displayable manager of a displayed lightbox."""
        viewNode = slicer.mrmlScene.GetSingletonNode(layoutName, "vtkMRMLSequenceLightboxViewNode")
        self.assertIsNotNone(viewNode)
        lightboxWidget = slicer.app.layoutManager().viewWidget(viewNode)
        self.assertIsNotNone(lightboxWidget)
        displayableManager = lightboxWidget.displayableManagerByClassName("vtkMRMLSequenceLightboxDisplayableManager")
        self.assertIsNotNone(displayableManager)
        return viewNode, lightboxWidget, displayableManager

    def sendEvent(self, interactor, eventId, x, y, shift=False):
        """Send a mouse event to the lightbox view, at a position in render window pixel coordinates."""
        interactor.SetEventPosition(int(x), int(y))
        interactor.SetShiftKey(1 if shift else 0)
        interactor.InvokeEvent(eventId)
        interactor.SetShiftKey(0)
        slicer.app.processEvents()

    def tileCenter(self, displayableManager, tileIndex, dx=0, dy=0):
        renderer = displayableManager.GetTileRenderer(tileIndex)
        origin = renderer.GetOrigin()
        size = renderer.GetSize()
        return (origin[0] + size[0] // 2 + dx, origin[1] + size[1] // 2 + dy)

    def rasToTilePosition(self, displayableManager, tileIndex, ras):
        """Return the render window position of a RAS point in a slice tile."""
        tileSliceNode = displayableManager.GetTileScene(tileIndex).GetSingletonNode("Red", "vtkMRMLSliceNode")
        rasToXY = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Invert(tileSliceNode.GetXYToRAS(), rasToXY)
        xy = rasToXY.MultiplyPoint(list(ras) + [1.0])
        origin = displayableManager.GetTileRenderer(tileIndex).GetOrigin()
        return (origin[0] + xy[0], origin[1] + xy[1])

    def checkOverlayRenderers(self, displayableManager):
        """Check that the additional renderers of the displayable managers of the tiles are rendered into the tiles.

        Some displayable managers render into a renderer of their own, on top of the renderer of the view (for example
        the interaction handles of markups). These renderers must cover the tile and not the whole lightbox view.
        """
        renderWindow = displayableManager.GetTileRenderer(0).GetRenderWindow()
        renderers = renderWindow.GetRenderers()
        overlayRenderers = [renderers.GetItemAsObject(index) for index in range(renderers.GetNumberOfItems())]
        overlayRenderers = [renderer for renderer in overlayRenderers if renderer.GetLayer() > 0]
        self.assertGreater(len(overlayRenderers), 0)
        for tileIndex in range(displayableManager.GetNumberOfTiles()):
            tileViewport = list(displayableManager.GetTileRenderer(tileIndex).GetViewport())
            overlaysInTile = [renderer for renderer in overlayRenderers if list(renderer.GetViewport()) == tileViewport]
            self.assertGreater(len(overlaysInTile), 0, f"tile {tileIndex} has no overlay renderer")
        fullViewOverlays = [renderer for renderer in overlayRenderers if list(renderer.GetViewport()) == [0.0, 0.0, 1.0, 1.0]]
        self.assertEqual(len(fullViewOverlays), 0)

    def test_SequenceLightboxInteraction(self):
        """Test interacting with the tiles of a slice lightbox.

        Changes of the view made in a tile are applied to the reference view, changes of a sequence
        item (markups) are stored in the sequence item that the tile shows.
        """
        self.delayDisplay("Starting the sequence lightbox interaction test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        markupsSequenceNode, markupsProxyNode = self.addMarkupsSequence(browserNode)
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)

        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        self.assertTrue(viewNode.GetInteractiveTiles())
        slicer.app.processEvents()
        lightboxView = lightboxWidget.sequenceLightboxView()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)
        interactor = lightboxView.interactor()
        redSliceNode = slicer.mrmlScene.GetSingletonNode("Red", "vtkMRMLSliceNode")

        self.delayDisplay("Scrolling in a tile scrolls the reference view, and therefore all tiles")
        offsetBefore = redSliceNode.GetSliceOffset()
        x, y = self.tileCenter(displayableManager, 0)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelForwardEvent, x, y)
        self.assertNotAlmostEqual(redSliceNode.GetSliceOffset(), offsetBefore)
        lightboxView.forceRender()
        slicer.app.processEvents()
        for tileIndex in range(6):
            tileSliceNode = displayableManager.GetTileScene(tileIndex).GetSingletonNode("Red", "vtkMRMLSliceNode")
            self.assertAlmostEqual(tileSliceNode.GetSliceOffset(), redSliceNode.GetSliceOffset(), places=3)

        self.delayDisplay("Zooming in a tile zooms the reference view")
        fieldOfViewBefore = redSliceNode.GetFieldOfView()[1]
        x, y = self.tileCenter(displayableManager, 1)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.RightButtonPressEvent, x, y)
        for step in range(1, 6):
            self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y + 8 * step)
        self.sendEvent(interactor, vtk.vtkCommand.RightButtonReleaseEvent, x, y + 40)
        self.assertNotAlmostEqual(redSliceNode.GetFieldOfView()[1], fieldOfViewBefore)
        # Slice view linking of the reference view is not affected by the tiles (which are not linked).
        redCompositeNode = slicer.app.applicationLogic().GetSliceLogic(redSliceNode).GetSliceCompositeNode()
        self.assertEqual(redCompositeNode.GetLinkedControl(), 1 if redCompositeNode.GetLinkedControl() else 0)

        self.delayDisplay("A click selects the item of the tile")
        x, y = self.tileCenter(displayableManager, 4)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x, y)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 4)
        # The pointer of a physical mouse often moves by a pixel or two during a click.
        x, y = self.tileCenter(displayableManager, 2)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 1, y + 1)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x + 2, y + 1)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 2)
        # A drag is not a click.
        x, y = self.tileCenter(displayableManager, 1)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        for step in range(1, 5):
            self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 10 * step, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x + 40, y)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 2)
        browserNode.SetSelectedItemNumber(0)
        slicer.app.processEvents()

        self.delayDisplay("A markup placed in a tile is stored in the item of that tile")
        interactionNode = slicer.app.applicationLogic().GetInteractionNode()
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActivePlaceNodeClassName("vtkMRMLMarkupsFiducialNode")
        selectionNode.SetActivePlaceNodeID(markupsProxyNode.GetID())
        interactionNode.SetPlaceModePersistence(0)
        interactionNode.SetCurrentInteractionMode(interactionNode.Place)
        slicer.app.processEvents()
        item3 = markupsSequenceNode.GetNthDataNode(3)
        self.assertEqual(item3.GetNumberOfControlPoints(), 1)
        x, y = self.tileCenter(displayableManager, 3)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x, y)
        self.assertEqual(item3.GetNumberOfControlPoints(), 2)
        # The proxy node shows item 0, so it is not modified, and placing does not select the item.
        self.assertEqual(markupsProxyNode.GetNumberOfControlPoints(), 1)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        # Non-persistent placement ends after one point, in the application as well.
        self.assertEqual(interactionNode.GetCurrentInteractionMode(), interactionNode.ViewTransform)
        # The point is placed at the position under the mouse pointer in that tile.
        placedPosition = [0.0, 0.0, 0.0]
        item3.GetNthControlPointPositionWorld(1, placedPosition)
        expectedX, expectedY = self.rasToTilePosition(displayableManager, 3, placedPosition)
        self.assertAlmostEqual(expectedX, x, delta=1.0)
        self.assertAlmostEqual(expectedY, y, delta=1.0)

        self.delayDisplay("A markup dragged in a tile moves in the item of that tile")
        item5 = markupsSequenceNode.GetNthDataNode(5)
        positionBefore = [0.0, 0.0, 0.0]
        item5.GetNthControlPointPositionWorld(0, positionBefore)
        # Center the reference view (and therefore the tiles) on the point, so that it is within
        # the tile whatever the size of the view.
        redSliceNode.JumpSliceByCentering(positionBefore[0], positionBefore[1], positionBefore[2])
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        x, y = self.rasToTilePosition(displayableManager, 5, positionBefore)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        for step in range(1, 6):
            self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 6 * step, y + 3 * step)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x + 30, y + 15)
        positionAfter = [0.0, 0.0, 0.0]
        item5.GetNthControlPointPositionWorld(0, positionAfter)
        self.assertGreater(vtk.vtkMath.Distance2BetweenPoints(positionBefore, positionAfter), 1.0)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        # The tile for the selected item shows the proxy node, so an edit made in that tile is
        # applied to the proxy node (and therefore to the regular views) as well.
        proxyPositionBefore = [0.0, 0.0, 0.0]
        markupsProxyNode.GetNthControlPointPositionWorld(0, proxyPositionBefore)
        redSliceNode.JumpSliceByCentering(proxyPositionBefore[0], proxyPositionBefore[1], proxyPositionBefore[2])
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        x, y = self.rasToTilePosition(displayableManager, 0, proxyPositionBefore)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        for step in range(1, 6):
            self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 6 * step, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x + 30, y)
        proxyPositionAfter = [0.0, 0.0, 0.0]
        markupsProxyNode.GetNthControlPointPositionWorld(0, proxyPositionAfter)
        self.assertGreater(vtk.vtkMath.Distance2BetweenPoints(proxyPositionBefore, proxyPositionAfter), 1.0)
        item0Position = [0.0, 0.0, 0.0]
        markupsSequenceNode.GetNthDataNode(0).GetNthControlPointPositionWorld(0, item0Position)
        self.assertAlmostEqual(vtk.vtkMath.Distance2BetweenPoints(item0Position, proxyPositionAfter), 0.0, places=3)

        self.delayDisplay("Shift + mouse wheel scrolls the grid (fixed first item) or steps through the items")
        x, y = self.tileCenter(displayableManager, 2)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        offsetBefore = redSliceNode.GetSliceOffset()
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelBackwardEvent, x, y, shift=True)
        self.assertEqual(viewNode.GetStartItemNumber(), 3)
        self.assertAlmostEqual(redSliceNode.GetSliceOffset(), offsetBefore)
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelForwardEvent, x, y, shift=True)
        self.assertEqual(viewNode.GetStartItemNumber(), 0)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFollowSelected)
        browserNode.SetSelectedItemNumber(1)
        slicer.app.processEvents()
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelBackwardEvent, x, y, shift=True)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 2)
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelForwardEvent, x, y, shift=True)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 1)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        browserNode.SetSelectedItemNumber(0)
        slicer.app.processEvents()

        self.delayDisplay("Non-interactive tiles only select the item")
        viewNode.SetInteractiveTiles(False)
        slicer.app.processEvents()
        offsetBefore = redSliceNode.GetSliceOffset()
        x, y = self.tileCenter(displayableManager, 2)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelForwardEvent, x, y)
        self.assertAlmostEqual(redSliceNode.GetSliceOffset(), offsetBefore)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x, y)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 2)
        x, y = self.tileCenter(displayableManager, 1)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 1, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x + 1, y)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 1)
        viewNode.SetInteractiveTiles(True)

        self.delayDisplay("Sequence lightbox interaction test passed")

    def test_SequenceLightboxTileRenderers(self):
        """Test that what the displayable managers of a tile render stays within the tile.

        Markups control points, labels and interaction handles (which are rendered into additional
        renderers of the render window) must be positioned relative to the viewport of the tile, and
        the additional renderers must be removed together with the tile.
        """
        self.delayDisplay("Starting the sequence lightbox tile renderers test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)

        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        slicer.app.processEvents()
        lightboxView = lightboxWidget.sequenceLightboxView()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)
        renderWindow = lightboxView.renderWindow()

        self.delayDisplay("A control point is drawn in every tile, at the same position within the tile")
        # A markups node of the scene (not of the sequence) is shown in every tile. Its control
        # point is placed near the center of the slice shown by the tiles.
        tileSliceNode = displayableManager.GetTileScene(0).GetSingletonNode("Red", "vtkMRMLSliceNode")
        tileSize = displayableManager.GetTileRenderer(0).GetSize()
        ras = tileSliceNode.GetXYToRAS().MultiplyPoint([tileSize[0] / 2 + 15.0, tileSize[1] / 2 + 10.0, 0.0, 1.0])[:3]
        markupsNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", "TestPoint")
        markupsNode.CreateDefaultDisplayNodes()
        displayNode = markupsNode.GetDisplayNode()
        # Labels are hidden before the control point exists, so that they are never rendered
        # (see addMarkupsSequence).
        displayNode.SetPointLabelsVisibility(False)
        displayNode.SetSelectedColor(1.0, 0.0, 0.0)
        displayNode.SetGlyphScale(5.0)
        markupsNode.AddControlPointWorld(ras)
        slicer.app.processEvents()
        image = self.renderedImage(displayableManager)
        dimensions = image.GetDimensions()
        for tileIndex in range(displayableManager.GetNumberOfTiles()):
            x, y = self.rasToTilePosition(displayableManager, tileIndex, ras)
            x = min(max(round(x), 0), dimensions[0] - 1)
            y = min(max(round(y), 0), dimensions[1] - 1)
            pixel = [int(image.GetScalarComponentAsFloat(x, y, 0, component)) for component in range(3)]
            self.assertGreater(pixel[0], 200, f"tile {tileIndex}: {pixel}")
            self.assertLess(pixel[1], 80, f"tile {tileIndex}: {pixel}")
            self.assertLess(pixel[2], 80, f"tile {tileIndex}: {pixel}")

        self.delayDisplay("Interaction handles are rendered into the viewport of the tile")
        roiNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsROINode", "TestROI")
        roiNode.CreateDefaultDisplayNodes()
        roiNode.GetDisplayNode().SetPointLabelsVisibility(False)  # see addMarkupsSequence
        roiNode.GetDisplayNode().SetHandlesInteractive(True)
        roiNode.SetCenter(ras)
        roiNode.SetSize(20.0, 20.0, 10.0)
        slicer.app.processEvents()
        self.renderedImage(displayableManager)
        self.checkOverlayRenderers(displayableManager)

        self.delayDisplay("The renderers of a tile are removed together with the tile")
        numberOfRenderers = renderWindow.GetRenderers().GetNumberOfItems()
        # The renderer of the lightbox view itself, plus the renderers of 6 tiles.
        self.assertEqual((numberOfRenderers - 1) % 6, 0)
        renderersPerTile = (numberOfRenderers - 1) // 6
        self.assertGreater(renderersPerTile, 1)
        viewNode.SetColumns(1)
        slicer.app.processEvents()
        self.renderedImage(displayableManager)
        self.assertEqual(displayableManager.GetNumberOfTiles(), 2)
        self.assertEqual(renderWindow.GetRenderers().GetNumberOfItems(), 1 + 2 * renderersPerTile)
        self.checkOverlayRenderers(displayableManager)
        viewNode.SetColumns(3)
        slicer.app.processEvents()
        self.renderedImage(displayableManager)
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)
        self.assertEqual(renderWindow.GetRenderers().GetNumberOfItems(), numberOfRenderers)
        self.checkOverlayRenderers(displayableManager)

        self.delayDisplay("Sequence lightbox tile renderers test passed")

    def test_SequenceLightboxSparseSequence(self):
        """Test editing a sequence that does not have an item for every index of the master sequence.

        A tile whose item is missing from a sequence shows the previous item (missing item mode
        "create from previous", the default) or nothing (other modes). Editing such a tile creates
        the missing item instead of modifying the previous one.
        """
        self.delayDisplay("Starting the sparse sequence test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)

        # Markups sequence with a single item, at the index value of the first master item.
        markupsSequenceNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceNode", "SparseMarkupsSequence")
        markupsSequenceNode.SetIndexName("frame")
        markupsNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", "sparseMarkups0")
        markupsNode.AddControlPoint(10.0, 5.0, 10.0)
        markupsSequenceNode.SetDataNodeAtValue(markupsNode, "0")
        slicer.mrmlScene.RemoveNode(markupsNode)
        browserNode.AddSynchronizedSequenceNodeID(markupsSequenceNode.GetID())
        browserNode.SetSaveChanges(markupsSequenceNode, False)
        slicer.modules.sequences.logic().UpdateProxyNodesFromSequences(browserNode)
        markupsProxyNode = browserNode.GetProxyNode(markupsSequenceNode)
        markupsProxyNode.CreateDefaultDisplayNodes()
        markupsProxyNode.GetDisplayNode().SetPointLabelsVisibility(False)  # see addMarkupsSequence

        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        browserNode.SetSelectedItemNumber(0)
        slicer.app.processEvents()
        lightboxView = lightboxWidget.sequenceLightboxView()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)
        interactor = lightboxView.interactor()

        self.delayDisplay("Tiles of missing items show the previous item")
        tile2Markups = displayableManager.GetTileScene(2).GetNodeByID(markupsProxyNode.GetID())
        tile3Markups = displayableManager.GetTileScene(3).GetNodeByID(markupsProxyNode.GetID())
        self.assertIsNotNone(tile2Markups)
        self.assertIsNotNone(tile3Markups)
        self.assertEqual(tile2Markups.GetNumberOfControlPoints(), 1)
        self.assertEqual(tile2Markups.GetDisplayNode().GetVisibility(), 1)
        self.assertEqual(tile3Markups.GetNumberOfControlPoints(), 1)

        self.delayDisplay("Placing a point in the tile of a missing item creates the item")
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        interactionNode = slicer.app.applicationLogic().GetInteractionNode()
        selectionNode.SetReferenceActivePlaceNodeClassName("vtkMRMLMarkupsFiducialNode")
        selectionNode.SetActivePlaceNodeID(markupsProxyNode.GetID())
        interactionNode.SetPlaceModePersistence(0)
        interactionNode.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.Place)
        slicer.app.processEvents()
        x, y = self.tileCenter(displayableManager, 2, dx=10, dy=-8)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        # The preview point that follows the pointer must not add an item to the sequence.
        self.assertEqual(markupsSequenceNode.GetNumberOfDataNodes(), 1)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x, y)
        self.assertEqual(interactionNode.GetCurrentInteractionMode(), slicer.vtkMRMLInteractionNode.ViewTransform)
        self.assertEqual(markupsSequenceNode.GetNumberOfDataNodes(), 2)
        item2 = markupsSequenceNode.GetDataNodeAtValue("2", True)
        self.assertIsNotNone(item2)
        self.assertEqual(item2.GetNumberOfControlPoints(), 2)
        # The previous item and the proxy node (which shows item 0) are not modified.
        self.assertEqual(markupsSequenceNode.GetDataNodeAtValue("0", True).GetNumberOfControlPoints(), 1)
        self.assertEqual(markupsProxyNode.GetNumberOfControlPoints(), 1)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)

        self.delayDisplay("The tiles of the following missing items show the new item")
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(tile2Markups.GetNumberOfControlPoints(), 2)
        self.assertEqual(tile3Markups.GetNumberOfControlPoints(), 2)
        self.assertEqual(displayableManager.GetTileScene(1).GetNodeByID(markupsProxyNode.GetID()).GetNumberOfControlPoints(), 1)

        self.delayDisplay("Missing items are hidden in the other missing item modes")
        browserNode.SetMissingItemMode(markupsSequenceNode, slicer.vtkMRMLSequenceBrowserNode.MissingItemDisplayHidden)
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(tile2Markups.GetDisplayNode().GetVisibility(), 1)
        self.assertEqual(tile3Markups.GetDisplayNode().GetVisibility(), 0)
        # Interacting with the tile of a hidden (missing) item must not hide the node in the
        # application: the visibility of the mirrored display node is derived, not written back.
        x, y = self.tileCenter(displayableManager, 3)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 4, y + 3)
        self.assertEqual(markupsProxyNode.GetDisplayNode().GetVisibility(), 1)
        self.assertEqual(tile3Markups.GetDisplayNode().GetVisibility(), 0)
        browserNode.SetMissingItemMode(markupsSequenceNode, slicer.vtkMRMLSequenceBrowserNode.MissingItemCreateFromPrevious)
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(tile3Markups.GetDisplayNode().GetVisibility(), 1)

        self.delayDisplay("Sparse sequence test passed")

    def test_SequenceLightboxCompleteNodePlacement(self):
        """Test place mode over a tile whose markups node already has all its control points.

        The markups displayable manager creates a new markups node when the active node is complete
        (for example an ROI with its two corners). The displayable managers of the tiles must not do
        that: the tile only mirrors the nodes of the scene, and a complete node is simply not
        available for placement in that tile.
        """
        self.delayDisplay("Starting the complete node placement test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)

        # ROI sequence with a complete ROI in the first item only.
        roiSequenceNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceNode", "ROISequence")
        roiSequenceNode.SetIndexName("frame")
        # A defined box ROI has a single control point (its center), which is also its maximum.
        roiNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsROINode", "roi0")
        roiNode.SetCenter(15.0, 12.0, 10.0)
        roiNode.SetSize(20.0, 15.0, 10.0)
        roiSequenceNode.SetDataNodeAtValue(roiNode, "0")
        slicer.mrmlScene.RemoveNode(roiNode)
        browserNode.AddSynchronizedSequenceNodeID(roiSequenceNode.GetID())
        browserNode.SetMissingItemMode(roiSequenceNode, slicer.vtkMRMLSequenceBrowserNode.MissingItemCreateFromDefault)
        slicer.modules.sequences.logic().UpdateProxyNodesFromSequences(browserNode)
        roiProxyNode = browserNode.GetProxyNode(roiSequenceNode)
        roiProxyNode.CreateDefaultDisplayNodes()
        roiProxyNode.GetDisplayNode().SetPointLabelsVisibility(False)  # see addMarkupsSequence
        self.assertEqual(roiProxyNode.GetNumberOfDefinedControlPoints(), roiProxyNode.GetMaximumNumberOfControlPoints())

        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        browserNode.SetSelectedItemNumber(0)
        slicer.app.processEvents()
        lightboxView = lightboxWidget.sequenceLightboxView()
        lightboxView.forceRender()
        slicer.app.processEvents()
        interactor = lightboxView.interactor()

        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        interactionNode = slicer.app.applicationLogic().GetInteractionNode()
        selectionNode.SetReferenceActivePlaceNodeClassName("vtkMRMLMarkupsROINode")
        selectionNode.SetActivePlaceNodeID(roiProxyNode.GetID())
        interactionNode.SetPlaceModePersistence(1)
        interactionNode.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.Place)
        slicer.app.processEvents()
        numberOfROINodes = len(slicer.util.getNodesByClass("vtkMRMLMarkupsROINode"))

        self.delayDisplay("Moving over the tile of a complete ROI does not create a new ROI node")
        x, y = self.tileCenter(displayableManager, 0)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 5, y + 5)
        self.assertEqual(len(slicer.util.getNodesByClass("vtkMRMLMarkupsROINode")), numberOfROINodes)
        self.assertEqual(selectionNode.GetActivePlaceNodeID(), roiProxyNode.GetID())
        self.assertEqual(interactionNode.GetCurrentInteractionMode(), slicer.vtkMRMLInteractionNode.Place)
        self.assertEqual(roiSequenceNode.GetNumberOfDataNodes(), 1)

        self.delayDisplay("The ROI of an empty item can still be placed")
        x, y = self.tileCenter(displayableManager, 1)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x - 20, y - 15)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x - 20, y - 15)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x - 20, y - 15)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x + 20, y + 15)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x + 20, y + 15)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x + 20, y + 15)
        self.assertEqual(len(slicer.util.getNodesByClass("vtkMRMLMarkupsROINode")), numberOfROINodes)
        item1 = roiSequenceNode.GetDataNodeAtValue("1", True)
        self.assertIsNotNone(item1)
        self.assertEqual(item1.GetNumberOfDefinedControlPoints(), item1.GetMaximumNumberOfControlPoints())
        self.assertGreater(min(item1.GetSize()[:2]), 1.0)
        self.assertAlmostEqual(roiSequenceNode.GetDataNodeAtValue("0", True).GetSize()[0], 20.0)
        interactionNode.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.ViewTransform)
        interactionNode.SetPlaceModePersistence(0)

        self.delayDisplay("Complete node placement test passed")

    def test_SequenceLightboxPlaybackUpdates(self):
        """Test that stepping through the items updates as few tiles as possible.

        When the selection changes, a tile that already shows an item that is still displayed is moved to
        the position of that item instead of being updated, and the tile that shows the selected item keeps
        showing the sequence item (the proxy node is only mirrored once it is modified).
        """
        self.delayDisplay("Starting the playback updates test")

        sequenceNode, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetRows(1)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFollowSelected)
        browserNode.SetSelectedItemNumber(0)
        slicer.app.processEvents()
        lightboxView = lightboxWidget.sequenceLightboxView()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual([displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(3)], [0, 1, 2])

        def tileRendererAddresses():
            return [displayableManager.GetTileRenderer(tileIndex).GetAddressAsString("vtkRenderer") for tileIndex in range(3)]

        def mirroredImageData(tileIndex):
            return displayableManager.GetTileScene(tileIndex).GetNodeByID(proxyNode.GetID()).GetImageData()

        # The number of times the slice image of a tile has to be recomputed is the number of
        # times the image data of its mirrored volume node is replaced.
        imageDataReplacements = [0, 0, 0]

        def imageDataReplaced(tileIndex):
            def callback(caller, event):
                imageDataReplacements[tileIndex] += 1
            return callback

        observedNodes = []
        for tileIndex in range(3):
            mirroredNode = displayableManager.GetTileScene(tileIndex).GetNodeByID(proxyNode.GetID())
            observedNodes.append((mirroredNode, mirroredNode.AddObserver(slicer.vtkMRMLVolumeNode.ImageDataModifiedEvent, imageDataReplaced(tileIndex))))

        self.delayDisplay("Selecting the next item moves the tiles")
        renderersBefore = tileRendererAddresses()
        browserNode.SetSelectedItemNumber(1)
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual([displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(3)], [1, 2, 3])
        renderersAfter = tileRendererAddresses()
        # The tiles of items 1 and 2 moved one position to the left, the tile of item 0 shows item 3 now.
        self.assertEqual(renderersAfter, [renderersBefore[1], renderersBefore[2], renderersBefore[0]])
        # The tile that showed item 0 (observed first) is the only one whose slice image had to be
        # recomputed: it shows item 3 now.
        self.assertEqual(imageDataReplacements, [1, 0, 0])
        for node, tag in observedNodes:
            node.RemoveObserver(tag)

        self.delayDisplay("The tile of the selected item shows the sequence item until the proxy node is modified")
        self.assertIs(mirroredImageData(0), sequenceNode.GetNthDataNode(1).GetImageData())
        self.assertIsNot(mirroredImageData(0), proxyNode.GetImageData())
        # An unsaved modification of the proxy node must be visible in the tile of the selected item.
        proxyNode.Modified()
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertIs(mirroredImageData(0), proxyNode.GetImageData())
        self.assertIs(mirroredImageData(1), sequenceNode.GetNthDataNode(2).GetImageData())
        browserNode.SetSelectedItemNumber(2)
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertIs(mirroredImageData(0), sequenceNode.GetNthDataNode(2).GetImageData())

        self.delayDisplay("Many modifications before a render are synchronized once")
        renderedValues = self.tileCenterValues(self.renderedImage(displayableManager), 1, 3)
        self.assertEqual(renderedValues, self.INTENSITIES[2:5])
        for itemNumber in range(5, -1, -1):
            browserNode.SetSelectedItemNumber(itemNumber)
        renderedValues = self.tileCenterValues(self.renderedImage(displayableManager), 1, 3)
        self.assertEqual(renderedValues, self.INTENSITIES[0:3])

        self.delayDisplay("Every render shows fully synchronized tiles")
        renderWindow = lightboxView.renderWindow()
        renderedItems = []

        def recordRenderedItems(caller, event):
            renderedItems.append([displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(displayableManager.GetNumberOfTiles())])

        observerTag = renderWindow.AddObserver(vtk.vtkCommand.StartEvent, recordRenderedItems)
        browserNode.SetSelectedItemNumber(3)
        # Let the render request of the selection change expire: a render request made while the
        # tiles are synchronized would then render the view immediately, with tiles that are only
        # partially synchronized.
        time.sleep(0.05)
        lightboxView.forceRender()
        slicer.app.processEvents()
        renderWindow.RemoveObserver(observerTag)
        expectedItems = [viewNode.GetItemNumberForTile(tileIndex) for tileIndex in range(displayableManager.GetNumberOfTiles())]
        self.assertGreaterEqual(len(renderedItems), 1)
        for items in renderedItems:
            self.assertEqual(items, expectedItems)

        self.delayDisplay("A custom list of items is limited to a reasonable length")
        viewNode.SetCustomItemNumbersFromString("0-100000000")
        self.assertEqual(len(viewNode.GetCustomItemNumbers()), 1000)
        viewNode.SetCustomItemNumbersFromString("3, 1-2")
        self.assertEqual(list(viewNode.GetCustomItemNumbers()), [3, 1, 2])

        self.delayDisplay("Playback updates test passed")

    def sendKey(self, interactor, keySym, shift=True):
        """Send a key press (and release) to the lightbox view, with the Shift modifier by default."""
        interactor.SetKeySym(keySym)
        interactor.SetShiftKey(1 if shift else 0)
        interactor.InvokeEvent(vtk.vtkCommand.KeyPressEvent)
        interactor.InvokeEvent(vtk.vtkCommand.KeyReleaseEvent)
        interactor.SetShiftKey(0)
        slicer.app.processEvents()

    def test_SequenceLightboxNavigation(self):
        """Test the tile spacing, the keyboard navigation and the hover frame of the lightbox."""
        self.delayDisplay("Starting the navigation test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFollowSelected)
        browserNode.SetSelectedItemNumber(0)
        slicer.app.processEvents()
        lightboxView = lightboxWidget.sequenceLightboxView()
        lightboxView.forceRender()
        slicer.app.processEvents()
        interactor = lightboxView.interactor()

        self.delayDisplay("The tiles are separated by the tile spacing")
        self.assertEqual(viewNode.GetTileSpacing(), 2)
        renderWindowSize = lightboxView.renderWindow().GetSize()
        tile0 = displayableManager.GetTileRenderer(0)
        tile1 = displayableManager.GetTileRenderer(1)
        # Tile 1 is right of tile 0, in the same row: the gap between them is the tile spacing.
        self.assertEqual(tile1.GetOrigin()[0] - (tile0.GetOrigin()[0] + tile0.GetSize()[0]), 2)
        self.assertEqual(tile0.GetOrigin()[1], tile1.GetOrigin()[1])
        viewNode.SetTileSpacing(10)
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(tile1.GetOrigin()[0] - (tile0.GetOrigin()[0] + tile0.GetSize()[0]), 10)
        # The three tiles of a row still fill the row (up to the pixels that cannot be split evenly).
        tile2 = displayableManager.GetTileRenderer(2)
        self.assertLess(renderWindowSize[0] - (tile2.GetOrigin()[0] + tile2.GetSize()[0]), 3)
        viewNode.SetTileSpacing(0)
        slicer.app.processEvents()
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual(tile1.GetOrigin()[0], tile0.GetOrigin()[0] + tile0.GetSize()[0])
        viewNode.SetTileSpacing(2)

        self.delayDisplay("Shift + keys step through the items")
        x, y = self.tileCenter(displayableManager, 0)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendKey(interactor, "Right")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 1)
        self.sendKey(interactor, "Right")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 2)
        self.sendKey(interactor, "Left")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 1)
        self.sendKey(interactor, "End")
        self.assertEqual(browserNode.GetSelectedItemNumber(), self.NUMBER_OF_ITEMS - 1)
        self.sendKey(interactor, "Home")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        # Page Down: one row of tiles (3 items) forward.
        self.sendKey(interactor, "Next")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 3)
        self.sendKey(interactor, "Prior")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        # Keys that are not used for navigation are ignored, and without Shift the arrow keys scroll
        # the slice of the tile (as in a regular slice view) instead of stepping through the items.
        self.sendKey(interactor, "a")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        redSliceNode = slicer.mrmlScene.GetSingletonNode("Red", "vtkMRMLSliceNode")
        sliceOffset = redSliceNode.GetSliceOffset()
        self.sendKey(interactor, "Right", shift=False)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        self.assertNotAlmostEqual(redSliceNode.GetSliceOffset(), sliceOffset)

        self.delayDisplay("With a fixed first item, Page Up/Down and Home/End scroll the grid")
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        self.sendKey(interactor, "Next")
        self.assertEqual(viewNode.GetStartItemNumber(), 3)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 0)
        self.sendKey(interactor, "Home")
        self.assertEqual(viewNode.GetStartItemNumber(), 0)
        self.sendKey(interactor, "End")
        self.assertEqual(viewNode.GetStartItemNumber(), 0)  # all 6 items fit in the grid
        self.assertEqual(browserNode.GetSelectedItemNumber(), self.NUMBER_OF_ITEMS - 1)
        browserNode.SetSelectedItemNumber(0)

        self.delayDisplay("The grid can be resized to show all items")
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFollowSelected)
        viewNode.SetRows(1)
        viewNode.SetColumns(1)
        showAllItemsButton = slicer.util.findChild(lightboxWidget, "ShowAllItemsButton")
        showAllItemsButton.click()
        slicer.app.processEvents()
        self.assertEqual(viewNode.GetItemSelectionMode(), slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        self.assertEqual(viewNode.GetStartItemNumber(), 0)
        self.assertGreaterEqual(viewNode.GetNumberOfTiles(), self.NUMBER_OF_ITEMS)
        self.assertLessEqual(viewNode.GetNumberOfTiles(), 2 * self.NUMBER_OF_ITEMS)
        lightboxView.forceRender()
        slicer.app.processEvents()
        self.assertEqual([displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(self.NUMBER_OF_ITEMS)], list(range(self.NUMBER_OF_ITEMS)))
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        slicer.app.processEvents()

        self.delayDisplay("The keyboard also works if the tiles are not interactive")
        viewNode.SetInteractiveTiles(False)
        slicer.app.processEvents()
        self.sendKey(interactor, "Right")
        self.assertEqual(browserNode.GetSelectedItemNumber(), 1)
        viewNode.SetInteractiveTiles(True)
        slicer.app.processEvents()

        self.delayDisplay("The tile under the pointer is framed")

        def tileBorderPixel(tileIndex):
            # Color of the left border of the tile, where the hover frame is drawn.
            image = self.renderedImage(displayableManager)
            renderer = displayableManager.GetTileRenderer(tileIndex)
            origin = renderer.GetOrigin()
            size = renderer.GetSize()
            x, y = origin[0], origin[1] + size[1] // 2
            return [int(image.GetScalarComponentAsFloat(x, y, 0, component)) for component in range(3)]

        # The border of the selected item would hide the frame.
        viewNode.SetHighlightSelectedItem(False)
        self.sendEvent(interactor, vtk.vtkCommand.LeaveEvent, -10, -10)
        unframed0 = tileBorderPixel(0)
        unframed1 = tileBorderPixel(1)
        x, y = self.tileCenter(displayableManager, 0)
        self.sendEvent(interactor, vtk.vtkCommand.EnterEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        framed0 = tileBorderPixel(0)
        self.assertNotEqual(framed0, unframed0)
        # Only the hovered tile is framed.
        self.assertEqual(tileBorderPixel(1), unframed1)
        x, y = self.tileCenter(displayableManager, 1)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.assertEqual(tileBorderPixel(0), unframed0)
        self.assertNotEqual(tileBorderPixel(1), unframed1)
        self.sendEvent(interactor, vtk.vtkCommand.LeaveEvent, -10, -10)
        self.assertEqual(tileBorderPixel(1), unframed1)
        # Tiles that are not interactive are not framed.
        viewNode.SetInteractiveTiles(False)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.assertEqual(tileBorderPixel(1), unframed1)
        viewNode.SetInteractiveTiles(True)
        viewNode.SetHighlightSelectedItem(True)

        self.delayDisplay("Navigation test passed")

    def test_SequenceTimeline(self):
        """Test the timeline layout: a strip of items centered on the current item, below the view."""
        self.delayDisplay("Starting the sequence timeline test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_TIMELINE_LAYOUT)
        slicer.app.processEvents()
        self.assertEqual(layoutManager.layout, self.SLICE_TIMELINE_LAYOUT)
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceTimeline1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        slicer.app.processEvents()

        self.delayDisplay("The timeline is a single row centered on the selected item")
        self.assertEqual(viewNode.GetRows(), 1)
        self.assertEqual(viewNode.GetColumns(), 7)
        self.assertEqual(viewNode.GetItemSelectionMode(), slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionCenterSelected)
        self.assertTrue(layoutManager.sliceWidget("Red").isVisible())
        self.assertTrue(lightboxWidget.isVisible())
        # The view that the tiles follow is above the strip.
        self.assertLess(layoutManager.sliceWidget("Red").mapToGlobal(qt.QPoint(0, 0)).y(), lightboxWidget.mapToGlobal(qt.QPoint(0, 0)).y())

        for selectedItemNumber in [0, 2, 5]:
            browserNode.SetSelectedItemNumber(selectedItemNumber)
            slicer.app.processEvents()
            lightboxWidget.sequenceLightboxView().forceRender()
            slicer.app.processEvents()
            self.assertEqual(displayableManager.GetTileItemNumber(3), selectedItemNumber)
            expectedItems = [itemNumber if 0 <= itemNumber < self.NUMBER_OF_ITEMS else -1 for itemNumber in range(selectedItemNumber - 3, selectedItemNumber + 4)]
            self.assertEqual([displayableManager.GetTileItemNumber(tileIndex) for tileIndex in range(7)], expectedItems)

        self.delayDisplay("Clicking a tile of the timeline selects its item")
        browserNode.SetSelectedItemNumber(2)
        slicer.app.processEvents()
        interactor = lightboxWidget.sequenceLightboxView().interactor()
        x, y = self.tileCenter(displayableManager, 5)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonPressEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.LeftButtonReleaseEvent, x, y)
        self.assertEqual(browserNode.GetSelectedItemNumber(), 4)

        self.delayDisplay("The 3D timeline layout follows the 3D view")
        layoutManager.setLayout(self.THREED_TIMELINE_LAYOUT)
        slicer.app.processEvents()
        viewNode3D, _, _ = self.lightboxView("SequenceTimeline3D")
        self.assertEqual(viewNode3D.GetReferenceViewNode().GetClassName(), "vtkMRMLViewNode")
        self.assertEqual(viewNode3D.GetRows(), 1)
        self.assertTrue(layoutManager.threeDWidget(0).isVisible())

        self.delayDisplay("Sequence timeline test passed")

    def updateLayoutFromReferenceView(self):
        """Let the Sequences module update the lightbox layouts after a reference view change.

        The layouts are updated when control returns to the event loop (first pass), and the
        widgets of the new layout become visible when the layout of the viewport is activated
        (second pass).
        """
        slicer.app.processEvents()
        slicer.app.processEvents()

    def test_SequenceLightboxReferenceViewLayout(self):
        """Test that the view displayed next to the lightbox is the reference view of the lightbox."""
        self.delayDisplay("Starting the lightbox reference view layout test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        redSliceNode = slicer.mrmlScene.GetSingletonNode("Red", "vtkMRMLSliceNode")
        greenSliceNode = slicer.mrmlScene.GetSingletonNode("Green", "vtkMRMLSliceNode")
        threeDViewNode = slicer.mrmlScene.GetSingletonNode("1", "vtkMRMLViewNode")
        self.assertEqual(viewNode.GetReferenceViewNodeID(), redSliceNode.GetID())
        self.assertTrue(redSliceNode.IsMappedInLayout())
        self.assertFalse(greenSliceNode.IsMappedInLayout())

        self.delayDisplay("Choosing another slice view as reference view displays that view next to the lightbox")
        viewNode.SetAndObserveReferenceViewNodeID(greenSliceNode.GetID())
        self.updateLayoutFromReferenceView()
        self.assertEqual(layoutManager.layout, self.SLICE_LIGHTBOX_LAYOUT)
        self.assertTrue(greenSliceNode.IsMappedInLayout())
        self.assertFalse(redSliceNode.IsMappedInLayout())
        self.assertTrue(layoutManager.sliceWidget("Green").isVisible())
        self.assertFalse(layoutManager.sliceWidget("Red").isVisible())
        # The lightbox keeps its widget and follows the green view.
        self.assertIs(layoutManager.viewWidget(viewNode), lightboxWidget)
        self.assertTrue(lightboxWidget.isVisible())
        self.assertLess(layoutManager.sliceWidget("Green").mapToGlobal(qt.QPoint(0, 0)).x(), lightboxWidget.mapToGlobal(qt.QPoint(0, 0)).x())
        self.renderedImage(displayableManager)
        self.assertEqual(displayableManager.GetTileMirror(0).GetTileViewNode().GetID(), greenSliceNode.GetID())

        self.delayDisplay("A 3D reference view is displayed next to the lightbox as well")
        viewNode.SetAndObserveReferenceViewNodeID(threeDViewNode.GetID())
        self.updateLayoutFromReferenceView()
        self.assertTrue(threeDViewNode.IsMappedInLayout())
        self.assertFalse(greenSliceNode.IsMappedInLayout())
        self.assertTrue(layoutManager.threeDWidget(0).isVisible())
        self.assertFalse(layoutManager.sliceWidget("Green").isVisible())
        self.assertTrue(lightboxWidget.isVisible())

        self.delayDisplay("Changing the reference view does not change another displayed layout")
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView)
        slicer.app.processEvents()
        viewNode.SetAndObserveReferenceViewNodeID(redSliceNode.GetID())
        self.updateLayoutFromReferenceView()
        self.assertEqual(layoutManager.layout, slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView)
        self.assertFalse(viewNode.IsMappedInLayout())
        self.assertFalse(threeDViewNode.IsMappedInLayout())

        self.delayDisplay("Showing the lightbox layout again displays the current reference view")
        layoutManager.setLayout(self.SLICE_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()
        self.assertTrue(redSliceNode.IsMappedInLayout())
        self.assertTrue(viewNode.IsMappedInLayout())
        self.assertFalse(threeDViewNode.IsMappedInLayout())

        self.delayDisplay("The timeline layout displays its own reference view above the strip")
        layoutManager.setLayout(self.SLICE_TIMELINE_LAYOUT)
        slicer.app.processEvents()
        timelineViewNode, timelineWidget, _ = self.lightboxView("SequenceTimeline1")
        self.assertEqual(timelineViewNode.GetReferenceViewNodeID(), redSliceNode.GetID())
        timelineViewNode.SetAndObserveReferenceViewNodeID(greenSliceNode.GetID())
        self.updateLayoutFromReferenceView()
        self.assertTrue(greenSliceNode.IsMappedInLayout())
        self.assertFalse(redSliceNode.IsMappedInLayout())
        self.assertLess(layoutManager.sliceWidget("Green").mapToGlobal(qt.QPoint(0, 0)).y(), timelineWidget.mapToGlobal(qt.QPoint(0, 0)).y())
        # Each lightbox view has its own reference view.
        self.assertEqual(viewNode.GetReferenceViewNodeID(), redSliceNode.GetID())

        self.delayDisplay("Closing the scene restores the default layouts")
        slicer.mrmlScene.Clear()
        slicer.app.processEvents()
        layoutManager.setLayout(self.SLICE_TIMELINE_LAYOUT)
        slicer.app.processEvents()
        self.assertEqual(timelineViewNode.GetReferenceViewNodeID(), redSliceNode.GetID())
        self.assertTrue(redSliceNode.IsMappedInLayout())
        self.assertFalse(greenSliceNode.IsMappedInLayout())

        self.delayDisplay("Lightbox reference view layout test passed")

    def test_SequenceLightboxOnly(self):
        """Test the layouts that only contain a lightbox: the reference view is hidden but still controls the tiles."""
        self.delayDisplay("Starting the lightbox-only layout test")

        _, browserNode, proxyNode = self.createSequenceBrowser()
        slicer.util.setSliceViewerLayers(background=proxyNode, fit=True)
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_LIGHTBOX_ONLY_LAYOUT)
        slicer.app.processEvents()
        self.assertEqual(layoutManager.layout, self.SLICE_LIGHTBOX_ONLY_LAYOUT)
        # The same lightbox view is used as in the side-by-side layout, so its settings are shared.
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceLightbox1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        slicer.app.processEvents()
        self.assertFalse(layoutManager.sliceWidget("Red").isVisible())
        self.assertTrue(lightboxWidget.isVisible())
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)
        lightboxWidget.sequenceLightboxView().forceRender()
        slicer.app.processEvents()

        self.delayDisplay("Scrolling a tile scrolls the hidden reference view")
        redSliceNode = slicer.mrmlScene.GetSingletonNode("Red", "vtkMRMLSliceNode")
        offsetBefore = redSliceNode.GetSliceOffset()
        interactor = lightboxWidget.sequenceLightboxView().interactor()
        x, y = self.tileCenter(displayableManager, 1)
        self.sendEvent(interactor, vtk.vtkCommand.MouseMoveEvent, x, y)
        self.sendEvent(interactor, vtk.vtkCommand.MouseWheelBackwardEvent, x, y)
        self.assertNotAlmostEqual(redSliceNode.GetSliceOffset(), offsetBefore)
        self.assertEqual(self.tileCenterValues(self.renderedImage(displayableManager), 2, 3), self.INTENSITIES)

        self.delayDisplay("The 3D lightbox-only layout shows the tiles of the 3D view")
        layoutManager.setLayout(self.THREED_LIGHTBOX_ONLY_LAYOUT)
        slicer.app.processEvents()
        viewNode3D, lightboxWidget3D, _ = self.lightboxView("SequenceLightbox3D")
        self.assertFalse(layoutManager.threeDWidget(0).isVisible())
        self.assertTrue(lightboxWidget3D.isVisible())
        self.assertEqual(viewNode3D.GetReferenceViewNode().GetClassName(), "vtkMRMLViewNode")

        self.delayDisplay("Lightbox-only layout test passed")

    def test_SequenceLightboxSceneSaveLoad(self):
        """Test that the configuration of a lightbox view is saved with the scene and restored."""
        import os
        import tempfile

        self.delayDisplay("Starting the sequence lightbox scene save/load test")

        _, browserNode, _ = self.createSequenceBrowser()
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.SLICE_TIMELINE_LAYOUT)
        slicer.app.processEvents()
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceTimeline1")
        viewNode.SetAndObserveSequenceBrowserNodeID(browserNode.GetID())
        viewNode.SetColumns(5)
        viewNode.SetItemStep(2)
        viewNode.SetShowItemLabels(False)
        viewNode.SetInteractiveTiles(False)
        browserNode.SetSelectedItemNumber(2)
        slicer.app.processEvents()
        referenceViewNodeID = viewNode.GetReferenceViewNodeID()
        self.assertIsNotNone(referenceViewNodeID)

        scenePath = os.path.join(tempfile.mkdtemp(), "SequenceLightboxSelfTest.mrb")
        self.assertTrue(slicer.util.saveScene(scenePath))

        self.delayDisplay("Closing the scene resets the lightbox view to a timeline")
        slicer.mrmlScene.Clear()
        slicer.app.processEvents()
        viewNode = slicer.mrmlScene.GetSingletonNode("SequenceTimeline1", "vtkMRMLSequenceLightboxViewNode")
        self.assertIsNotNone(viewNode)
        self.assertEqual(viewNode.GetRows(), 1)
        self.assertEqual(viewNode.GetItemSelectionMode(), slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionCenterSelected)
        self.assertIsNone(viewNode.GetSequenceBrowserNode())

        self.delayDisplay("Loading the scene restores the lightbox view configuration")
        self.assertTrue(slicer.util.loadScene(scenePath))
        slicer.app.processEvents()
        self.assertEqual(layoutManager.layout, self.SLICE_TIMELINE_LAYOUT)
        viewNode, lightboxWidget, displayableManager = self.lightboxView("SequenceTimeline1")
        self.assertEqual(viewNode.GetRows(), 1)
        self.assertEqual(viewNode.GetColumns(), 5)
        self.assertEqual(viewNode.GetItemStep(), 2)
        self.assertEqual(viewNode.GetItemSelectionMode(), slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionCenterSelected)
        self.assertFalse(viewNode.GetShowItemLabels())
        self.assertFalse(viewNode.GetInteractiveTiles())
        self.assertIsNotNone(viewNode.GetSequenceBrowserNode())
        self.assertEqual(viewNode.GetSequenceBrowserNode().GetName(), browserNode.GetName())
        self.assertIsNotNone(viewNode.GetReferenceViewNode())
        self.assertEqual(viewNode.GetReferenceViewNodeID(), referenceViewNodeID)
        lightboxWidget.sequenceLightboxView().forceRender()
        slicer.app.processEvents()
        self.assertEqual(displayableManager.GetNumberOfTiles(), 5)
        self.assertEqual(displayableManager.GetTileItemNumber(2), viewNode.GetSequenceBrowserNode().GetSelectedItemNumber())

        self.delayDisplay("Sequence lightbox scene save/load test passed")

    def createModelSequenceBrowser(self):
        """Create a sequence of spheres, each item with a different radius."""
        import vtk

        sequenceNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceNode", "TestModelSequence")
        for itemIndex in range(self.NUMBER_OF_ITEMS):
            sphere = vtk.vtkSphereSource()
            sphere.SetRadius(10.0 + 6.0 * itemIndex)
            sphere.Update()
            modelNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLModelNode", f"sphere{itemIndex}")
            modelNode.SetAndObservePolyData(sphere.GetOutput())
            sequenceNode.SetDataNodeAtValue(modelNode, str(itemIndex))
            slicer.mrmlScene.RemoveNode(modelNode)

        browserNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSequenceBrowserNode", "TestModelBrowser")
        browserNode.SetAndObserveMasterSequenceNodeID(sequenceNode.GetID())
        slicer.modules.sequences.logic().UpdateProxyNodesFromSequences(browserNode)
        proxyNode = browserNode.GetProxyNode(sequenceNode)
        proxyNode.CreateDefaultDisplayNodes()
        return sequenceNode, browserNode, proxyNode

    def renderedPixels(self, displayableManager):
        """Return the rendered content of the lightbox view as a flat array."""
        import vtk.util.numpy_support as ns

        return ns.vtk_to_numpy(self.renderedImage(displayableManager).GetPointData().GetScalars()).copy()

    def test_SequenceLightbox3D(self):
        """Test the lightbox of a 3D view.

        The tiles of a 3D lightbox follow the camera of the reference 3D view. This has to work
        without interacting with the lightbox itself.
        """
        self.delayDisplay("Starting the 3D sequence lightbox test")

        _, _, proxyNode = self.createModelSequenceBrowser()
        self.assertIsNotNone(proxyNode)

        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(self.THREED_LIGHTBOX_LAYOUT)
        slicer.app.processEvents()

        viewNode = slicer.mrmlScene.GetSingletonNode("SequenceLightbox3D", "vtkMRMLSequenceLightboxViewNode")
        self.assertIsNotNone(viewNode)
        viewNode.SetRows(2)
        viewNode.SetColumns(3)
        viewNode.SetItemSelectionMode(slicer.vtkMRMLSequenceLightboxViewNode.ItemSelectionFixed)
        viewNode.SetStartItemNumber(0)
        slicer.app.processEvents()

        lightboxWidget = layoutManager.viewWidget(viewNode)
        self.assertIsNotNone(lightboxWidget)
        displayableManager = lightboxWidget.displayableManagerByClassName("vtkMRMLSequenceLightboxDisplayableManager")
        self.assertIsNotNone(displayableManager)
        self.assertEqual(displayableManager.GetNumberOfTiles(), 6)

        threeDView = layoutManager.threeDWidget(0).threeDView()
        threeDView.resetFocalPoint()
        slicer.app.processEvents()
        self.renderedPixels(displayableManager)
        self.checkOverlayRenderers(displayableManager)

        self.delayDisplay("Each tile shows a different item")
        tileScene = displayableManager.GetTileScene(0)
        firstItemModel = tileScene.GetNodeByID(proxyNode.GetID())
        lastItemModel = displayableManager.GetTileScene(5).GetNodeByID(proxyNode.GetID())
        self.assertIsNotNone(firstItemModel)
        self.assertIsNotNone(lastItemModel)
        firstBounds = firstItemModel.GetPolyData().GetBounds()
        lastBounds = lastItemModel.GetPolyData().GetBounds()
        self.assertGreater(lastBounds[1] - lastBounds[0], firstBounds[1] - firstBounds[0] + 1.0)

        self.delayDisplay("Mirrored proxy nodes are visible")
        # A tile is first updated before its item number is known. The visibility of the proxy
        # nodes must be recomputed afterwards, otherwise the tiles stay empty.
        mirroredDisplayNode = tileScene.GetNodeByID(proxyNode.GetDisplayNode().GetID())
        self.assertIsNotNone(mirroredDisplayNode)
        self.assertEqual(mirroredDisplayNode.GetVisibility(), 1)

        self.delayDisplay("The tiles follow the camera of the reference view without interaction")
        before = self.renderedPixels(displayableManager)
        cameraNode = threeDView.cameraNode()
        self.assertIsNotNone(cameraNode)
        position = list(cameraNode.GetPosition())
        cameraNode.SetPosition(position[1] + 120.0, position[0] - 90.0, position[2] + 60.0)
        cameraNode.ResetClippingRange()
        slicer.app.processEvents()
        after = self.renderedPixels(displayableManager)
        self.assertNotEqual(int((before != after).sum()), 0)

        tileCameraNode = tileScene.GetSingletonNode("1", "vtkMRMLCameraNode")
        self.assertIsNotNone(tileCameraNode)
        for tileValue, referenceValue in zip(tileCameraNode.GetPosition(), cameraNode.GetPosition(), strict=True):
            self.assertAlmostEqual(tileValue, referenceValue, places=3)

        self.delayDisplay("Hiding a node updates the tiles")
        proxyNode.GetDisplayNode().SetVisibility(False)
        slicer.app.processEvents()
        hidden = self.renderedPixels(displayableManager)
        self.assertNotEqual(int((hidden != after).sum()), 0)
        self.assertEqual(mirroredDisplayNode.GetVisibility(), 0)

        proxyNode.GetDisplayNode().SetVisibility(True)
        slicer.app.processEvents()
        self.renderedPixels(displayableManager)
        self.assertEqual(mirroredDisplayNode.GetVisibility(), 1)

        self.delayDisplay("The tiles use the background of the reference 3D view")
        referenceViewNode = viewNode.GetReferenceViewNode()
        backgroundColor = (0.1, 0.2, 0.3)
        backgroundColor2 = (0.6, 0.5, 0.4)
        referenceViewNode.SetBackgroundColor(backgroundColor)
        referenceViewNode.SetBackgroundColor2(backgroundColor2)
        # The background of the lightbox view (visible in the gaps and in the place of empty tiles)
        # is set when the lightbox view node is modified. This must not reset the background of the
        # tiles, which share the render window of the lightbox view.
        viewNode.SetShowItemLabels(False)
        slicer.app.processEvents()
        image = self.renderedImage(displayableManager)
        for tileIndex in range(displayableManager.GetNumberOfTiles()):
            tileRenderer = displayableManager.GetTileRenderer(tileIndex)
            self.assertTrue(tileRenderer.GetGradientBackground(), f"tile {tileIndex}")
            for value, expected in zip(tileRenderer.GetBackground(), backgroundColor, strict=True):
                self.assertAlmostEqual(value, expected, places=6, msg=f"tile {tileIndex}")
            for value, expected in zip(tileRenderer.GetBackground2(), backgroundColor2, strict=True):
                self.assertAlmostEqual(value, expected, places=6, msg=f"tile {tileIndex}")
            # The bottom of the gradient has the first background color. The pixel is taken from the
            # bottom-right corner of the tile, inside the frame drawn around the selected tile.
            origin = tileRenderer.GetOrigin()
            size = tileRenderer.GetSize()
            x, y = origin[0] + size[0] - 6, origin[1] + 6
            pixel = [image.GetScalarComponentAsFloat(x, y, 0, component) for component in range(3)]
            for value, expected in zip(pixel, backgroundColor, strict=True):
                self.assertAlmostEqual(value, expected * 255.0, delta=8.0, msg=f"tile {tileIndex}: {pixel}")
        # The lightbox view keeps its own background.
        lightboxRenderer = lightboxWidget.sequenceLightboxView().renderWindow().GetRenderers().GetFirstRenderer()
        self.assertFalse(lightboxRenderer.GetGradientBackground())
        for value, expected in zip(lightboxRenderer.GetBackground(), viewNode.GetBackgroundColor(), strict=True):
            self.assertAlmostEqual(value, expected, places=6)

        self.delayDisplay("3D sequence lightbox test passed")
