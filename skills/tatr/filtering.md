# Filtering

Use `tatr ls -f '<query>'`; prefer it over `grep`.

| Field | Operators |
|---|---|
| `:status`, `:kind`, `:flow_step`, `:plan_status` | `eq`, `in [...]` |
| `:priority` | `eq` |
| `:title` | `eq`, `contains` |
| `:tags`, `:depends` | `contains` |
| `:parent` | `eq` |

Connectives: `and`, `or`, `not`. Parentheses group expressions.

```bash
tatr ls -f ':status eq OPEN' --sort priority
tatr ls -f '(:priority eq 100) and (:tags contains feature)'
tatr ls -f ':kind in [TASK, STORY]'
tatr ls -f ':parent eq 20260730-153122'
tatr ls -f ':depends contains 20260730-153325'
```

Filtering composes with `--sort` and `-R`. Literals support version and
hyphenated tags such as `v0.8.0` and `release-candidate`.
