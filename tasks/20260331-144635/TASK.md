# Implement a filter system for tasks

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: feature

We need to have a way of filtering tasks by status and tags. We can imagine
something like having `-s open, closed etc` and `-t tag1, etc` but it would be
way cooler to have a `query filter language` SQL style that we can use.

We need to think about a syntax for it. Should be as simple as possible.

We need operators like `and`, `or` and `not`.
We also need some stuff like `eq` or `contains` or `in`. And that means we also
need parenthesis for defining lists. Maybe we can have `[` `]` lists using
brackets and `(` `)` normal parens for doing order of operations.

So for example we can have this filter

```
(:status eq OPEN) and (feature in :tags)
```

This should show us the open tasks that have the feature tag.

Now quick question. Should we include sort as part of the query or should it
stay separate? Uhm... yeah why not separate. It works fine.

So the tatr command would look like this:

```console
tatr ls -s priority -f ":status eq OPEN and feature in :tags"
```

The idea is that `:` is used for keywords from the task. For example we will
have `status` and `tags` for now. But maybe we can have even `:title` at some
point. Altough for the title it is easier because you can filter it in
telescope. Or in `fzf` if you want to use the terminal.
