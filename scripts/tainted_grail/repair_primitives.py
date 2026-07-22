#!/usr/bin/env python3
"""Compatibility facade for the reusable engine-neutral ``semantic_repair`` package.

This module intentionally exposes no game, loader, assembly, reflection, network,
subprocess, deployment, or evidence-promotion capability.
"""

from semantic_repair import *  # noqa: F401,F403
