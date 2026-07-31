# Tatr Filtering

`tatr ls -f` filters over task fields:

- `:status`
- `:priority`
- `:title`
- `:tags`
- `:kind`
- `:flow_step`
- `:plan_status`
- `:parent`
- `:depends`

Operators are `eq`, `contains`, and `in` with `[...]` lists. Connectives are
`and`, `or`, and `not`; use parentheses for grouping.

```bash
tatr ls -f '(:status eq OPEN)'
tatr ls -f ':tags contains feature'
tatr ls -f '(:status eq OPEN) and (:tags contains feature)'
tatr ls -f ':tags contains v0.8.0' --sort priority
tatr ls -f ':kind eq EPIC'
tatr ls -f '(:plan_status eq APPROVED) and (:flow_step in [BACKLOG, PLANNED])'
tatr ls -f ':parent eq 20260730-153122'
tatr ls -f ':depends contains 20260730-153325'
```

Enum-valued fields (`:status`, `:kind`, `:flow_step`, `:plan_status`) take
`eq` and `in`; `:parent` takes `eq`; `:tags` and `:depends` take `contains`.

Filtering composes with `-s/--sort` and `-R`. Prefer `-f` over piping `tatr ls`
through `grep`.
