# Escape dash inside of strings title since that can cause some issues

- PRIORITY: 100
- TAGS: bug, historical
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE

When I try to add `-` in the title it bugs out. Might be an issue with
`argparse.h` will have to investigate.

Somehow this works as expected now - will close it.
