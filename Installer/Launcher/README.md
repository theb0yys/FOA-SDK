# Installed launcher

This directory owns launchers shipped as part of an installed FOA-SDK suite.

The current Windows launcher verifies the installed manifest, Editor binary, and dedicated project before starting the Editor. It supports a bounded self-test used by installer smoke validation.

An installed launcher may open the FOA-SDK Editor only. It may not launch FoA, deploy mods, invoke runtime adapters, mutate saves, sign artifacts, publish releases, or bypass installed-payload verification.
