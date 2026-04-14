# In recursive mode we should not display empty listings

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: bug

When we use `-r` with the `ls` subcommand and we also apply a filter with `-f`
we might have sections that are empty. In that case we should not display the
section title anymore.
