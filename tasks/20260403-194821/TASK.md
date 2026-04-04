# Centralize tasks by adding a recursive mode

- STATUS: CLOSED
- PRIORITY: 90
- TAGS: feature

I would like to have a flag like `-R, --recursive` for the `ls` command that
will search in all subdirectories for `tasks` folder and list all of them,
maybe with some title or section format.

```
/home/alex/personal/tatr
... then tasks from tatr
/home/alex/personal/aids.h
... then tasks from aids.h
```

And filters and sorting should be done on each section not on all of them at
once (here it is more relevant for sorting).

Why this is really cool is that I will be able to see the tasks for all my
projects so I will easily know what to work on next. And obviously if a project
becomes out of scope or boring or whatever I can just delete it.
