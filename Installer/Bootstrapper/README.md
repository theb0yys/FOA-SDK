# Installer bootstrapper

This directory owns bounded prerequisite detection, reviewed package acquisition, hash and policy verification, and the transition into the suite wizard or package engine.

The bootstrapper must fail closed for unsupported hosts, missing prerequisites, source or hash drift, unsafe paths, dependency cycles, unreviewed redistribution, partial acquisition, unexpected elevation, and stale suite definitions.

Network access, process execution, and elevation are explicit reviewed capabilities. They are never inferred from package presence. The bootstrapper grants no FoA launch, deployment, runtime, save, signing, publication, catalog-mutation, or evidence-promotion authority.
