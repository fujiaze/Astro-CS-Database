"""Shared helpers for the independent ACSD v2 validation tree.

This package never imports anything from ``src/`` and never reaches into the
production solver.  It only provides JSON/IO plumbing, exact rational
arithmetic, provenance capture and Monte-Carlo statistics helpers so that the
per-case truth scripts stay short and auditable.
"""
