# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Scoped UnityPy 1.24.2 writer correction for explicit null managed references.

The pinned reader omits ReferencedObjectData when the reference class is empty;
the pinned writer otherwise indexes the absent data member. Only the inspected
null sentinel with an empty type identity is handled here. No source record or
installed parser file is modified. Use in the isolated, single-threaded audit.
"""
from contextlib import contextmanager


@contextmanager
def null_reference_writer(helper, version):
    if version != "1.24.2":
        raise ValueError("Null-reference writer adapter requires UnityPy 1.24.2.")
    original = helper.write_value
    counts = {"null_references": 0}

    def write(value, node, writer, config):
        is_null = (node.m_Type == "ReferencedObject" and isinstance(value, dict)
                   and value.get("rid") == -2 and type(value.get("rid")) is int
                   and value.get("type") == {"class": "", "ns": "", "asm": ""})
        if not is_null:
            return original(value, node, writer, config)
        data_nodes = [child for child in node.m_Children if child.m_Type == "ReferencedObjectData"]
        if len(data_nodes) != 1 or data_nodes[0].m_Name in value:
            raise ValueError("Unexpected data on a null managed-reference record.")
        # Ask the same embedded-schema resolver as the reader to confirm absence.
        if helper.get_ref_type_node(value, config.assetsfile) is not None:
            raise ValueError("Null managed reference unexpectedly resolved to a data schema.")
        for child in node.m_Children:
            if child is not data_nodes[0]:
                original(value[child.m_Name], child, writer, config)
        if helper.metaflag_is_aligned(node.m_MetaFlag):
            writer.align_stream()
        counts["null_references"] += 1

    helper.write_value = write
    try:
        yield counts
    finally:
        helper.write_value = original
