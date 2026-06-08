"""Allow ``python -m lgtv_easy`` to run the CLI (and GUI when no args)."""
import sys

from .cli import main

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
