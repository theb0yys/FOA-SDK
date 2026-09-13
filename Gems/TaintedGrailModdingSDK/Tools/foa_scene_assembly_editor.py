# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native O3DE consumer for bounded source-hierarchy batches.

Call start_import with an already prepared private batch. The modal progress
window isolates the single undo transaction while creation/rollback yields to Qt.
This module neither loads a game nor supplies missing renderer/component behavior.
"""
import json
import struct
from PySide6 import QtCore, QtWidgets
import azlmbr.bus as bus
import azlmbr.components as components
import azlmbr.editor as editor
import azlmbr.entity as entity
import azlmbr.foa as foa
import azlmbr.math as math
from foa_scene_assembly import AssemblyJob, require
from foa_scene_render_assembly import MAX_DRAWS, placement_digest, validate_render_batch


def placement(eid, event, *args):
    return foa.SourceScenePlacementBus(bus.Event, event, eid, *args)


def info(eid, event):
    return editor.EditorEntityInfoRequestBus(bus.Event, event, eid)


def matrix_bits(eid):
    return tuple(placement(eid, 'GetWorldMatrixBits'))


def source_key(descriptor):
    ident = json.loads(descriptor)['identity']
    if 'archive_sha256' in ident:
        return 'merged', ident['archive_sha256'], ident['member_guid'], ident['member_sha256'], ident['instance_ordinal']
    return 'scene', ident['bundle_sha256'], ident['serialized_file'], ident['path_id']


class NativeHierarchyAdapter:
    def __init__(self, parent):
        self.parent = parent
        types = editor.EditorComponentAPIBus(bus.Broadcast, 'FindComponentTypeIdsByEntityType',
                                            ['Source scene placement'], entity.EntityType().Game)
        require(len(types) == 1 and not types[0].IsNull(), 'Native source placement component is unavailable.')
        self.component_type = types[0]

    def preflight(self, rows):
        require(editor.ToolsApplicationRequestBus(bus.Broadcast, 'EntityExists', self.parent), 'Assembly parent is absent.')
        anchor = components.TransformBus(bus.Event, 'GetWorldTM', self.parent)
        columns = (anchor.GetBasisX(), anchor.GetBasisY(), anchor.GetBasisZ(), anchor.GetTranslation())
        identity = tuple(col.GetElement(r) for r in range(3) for col in columns) == (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0.)
        require(identity and not placement(self.parent, 'GetSource'),
                'Import requires an unbound parent with an identity world transform.')
        wanted = {source_key(row.descriptor) for row in rows}
        existing = entity.SearchBus(bus.Broadcast, 'SearchEntities', entity.SearchFilter())
        require(len(existing) <= 500000, 'Native entity inventory exceeds assembly preflight bounds.')
        for eid in existing:
            result = editor.EditorComponentAPIBus(bus.Broadcast, 'GetComponentOfType', eid, self.component_type)
            if result.IsSuccess():
                descriptor = placement(eid, 'GetSource')
                require(not descriptor or source_key(descriptor) not in wanted, 'Source objects already exist in this level.')

    def create(self, parent):
        eid = editor.ToolsApplicationRequestBus(bus.Broadcast, 'CreateNewEntityAtPosition', math.Vector3(),
                                               parent if parent is not None else self.parent)
        require(eid.IsValid(), 'Native entity creation failed.')
        return eid

    def configure(self, eid, row):
        editor.EditorEntityAPIBus(bus.Event, 'SetName', eid, row.name)
        world = struct.unpack('<16f', struct.pack('<16I', *row.host_world_bits))
        components.TransformBus(bus.Event, 'SetWorldTranslation', eid, math.Vector3(world[3], world[7], world[11]))
        result = editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, [self.component_type])
        require(result.IsSuccess() and placement(eid, 'BindSource', row.descriptor),
                'Native source placement binding failed for '+str(row.key))
        editor.EditorEntityAPIBus(bus.Event, 'SetVisibilityState', eid, row.active_in_hierarchy)
        require(placement(eid, 'CommitAssemblyState'), 'Native prefab assembly state could not be committed.')

    def verify(self, eid, row, parent):
        require(placement(eid, 'GetStatus') == 'READY' and placement(eid, 'GetSource') == row.descriptor,
                'Native source binding readback differs.')
        require(info(eid, 'GetParent') == (parent if parent is not None else self.parent), 'Native source parent differs.')
        require(info(eid, 'GetName') == row.name, 'Native source name differs for '+str(row.key))
        require(info(eid, 'IsVisible') == row.active_in_hierarchy, 'Native source visibility differs for '+str(row.key))
        actual = matrix_bits(eid)
        if actual != row.host_world_bits:
            position = components.TransformBus(bus.Event, 'GetWorldTM', eid).GetTranslation()
            anchor_bits = struct.unpack('<3I', struct.pack('<3f', *(position.GetElement(i) for i in range(3))))
            require(False, 'Native unchanged source matrix bits differ for '+str(row.key)+
                    ': actual='+str(actual)+' expected='+str(row.host_world_bits)+
                    ' anchor='+str(anchor_bits)+' baseline='+str(row.native_anchor_bits))

    def remove(self, eid):
        editor.ToolsApplicationRequestBus(bus.Broadcast, 'DeleteEntityById', eid)
        require(not editor.ToolsApplicationRequestBus(bus.Broadcast, 'EntityExists', eid), 'Native rollback could not remove an entity.')


class NativeRenderedHierarchyAdapter(NativeHierarchyAdapter):
    def __init__(self, parent, rendering):
        super().__init__(parent)
        self.rendering = rendering
        types = editor.EditorComponentAPIBus(bus.Broadcast, 'FindComponentTypeIdsByEntityType',
                                            ['Source scene rendering'], entity.EntityType().Game)
        require(len(types) == 1 and not types[0].IsNull(), 'Native source rendering component is unavailable.')
        self.render_type = types[0]
        self.expected = {}

    def preflight(self, rows):
        super().preflight(rows)
        statistics = json.loads(foa.SourceShaderRenderBus(bus.Broadcast, 'GetStatistics'))
        require(statistics['entity_draws']+self.rendering.draw_count <= MAX_DRAWS, 'Native render capacity is unavailable.')
        self.expected = {row.key: self.rendering.bindings.get(placement_digest(row)) for row in rows}

    def configure(self, eid, row):
        super().configure(eid, row)
        binding = self.expected[row.key]
        if binding is None:
            return
        result = editor.EditorComponentAPIBus(bus.Broadcast, 'AddComponentsOfType', eid, [self.render_type])
        require(result.IsSuccess() and foa.SourceSceneRenderBus(bus.Event, 'BindDraw', eid, binding),
                'Native source render binding was rejected.')
        require(placement(eid, 'CommitAssemblyState'), 'Native render prefab state could not be committed.')

    def verify(self, eid, row, parent):
        super().verify(eid, row, parent)
        binding = self.expected[row.key]
        if binding is not None:
            require(foa.SourceSceneRenderBus(bus.Event, 'GetBinding', eid) == binding, 'Native render binding readback differs.')

    def ready(self, eid, row):
        if self.expected[row.key] is None:
            return True
        status = foa.SourceSceneRenderBus(bus.Event, 'GetStatus', eid)
        require(status in ('READY', 'LOADING'), 'Native source rendering failed: '+str(status))
        return status == 'READY'


class ImportProgress(QtWidgets.QDialog):
    """Keep the modal boundary alive until cancellation has finished rollback."""
    def __init__(self, maximum):
        super().__init__()
        self.cancel_requested = False
        self.setWindowTitle('Import source scene objects')
        self.setWindowModality(QtCore.Qt.ApplicationModal)
        layout = QtWidgets.QVBoxLayout(self)
        self.label = QtWidgets.QLabel('Creating source scene objects...', self)
        self.progress = QtWidgets.QProgressBar(self); self.progress.setRange(0, maximum)
        self.button = QtWidgets.QPushButton('Cancel', self)
        self.button.clicked.connect(self.cancel)
        layout.addWidget(self.label); layout.addWidget(self.progress); layout.addWidget(self.button)

    def cancel(self):
        self.cancel_requested = True
        self.label.setText('Removing the incomplete import...')
        self.button.setEnabled(False)

    def reject(self):
        self.cancel()


_active_import = None


def start_import(raw, parent, on_finished, *, render_batch=None):
    """Import a validated batch as one undoable operation. Return its controller.

    The callback receives the job including explicit failure/recovery IDs. The
    caller must keep the private batch for source/unsupported-component context.
    """
    global _active_import
    require(_active_import is None, 'A source hierarchy import is already running.')
    require(callable(on_finished), 'Assembly completion callback is required.')
    rendering = validate_render_batch(raw, render_batch) if render_batch is not None else None
    adapter = NativeRenderedHierarchyAdapter(parent, rendering) if rendering is not None else NativeHierarchyAdapter(parent)
    job = AssemblyJob(raw, adapter, ready=adapter.ready if rendering is not None else None)
    dialog = ImportProgress(len(job.rows))
    job.cancelled = lambda: dialog.cancel_requested
    controller = {'job': job, 'dialog': dialog}
    _active_import = controller
    editor.ToolsApplicationRequestBus(bus.Broadcast, 'BeginUndoBatch', 'Import source scene objects')
    dialog.show()

    def step():
        global _active_import
        state = job.step()
        if state == 'ROLLING_BACK':
            dialog.label.setText('Removing the incomplete import...')
            dialog.button.setEnabled(False)
        elif state == 'WAITING':
            dialog.label.setText('Loading source materials...')
            dialog.progress.setValue(len(job.created))
        else:
            dialog.progress.setValue(len(job.created))
        if state in ('PASSED', 'FAILED', 'CANCELLED', 'CLEANUP_FAILED'):
            editor.ToolsApplicationRequestBus(bus.Broadcast, 'EndUndoBatch')
            dialog.done(0); dialog.deleteLater(); _active_import = None
            on_finished(job)
        else:
            QtCore.QTimer.singleShot(50 if state == 'WAITING' else 0, step)
    QtCore.QTimer.singleShot(0, step)
    return controller
