"""Shared paths for the standalone build; generators write only under build/."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / 'assets'
BUILD = ROOT / 'build'
GENERATED = BUILD / 'generated'
GENERATED.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ASSETS))
