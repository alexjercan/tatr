# Implement ls subcommand

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: feature

The `ls` subcommand should list all the tasks. We should be able to see the
titles, status, priority and tags of the tasks. We should also be able to see
the filename so that we can easily open the file and see the details. The
format of the output should look like:

```
Implement ls subcommand (status: OPEN, priority: 100, tags: feature) - tasks/20260330-202358/TASK.md
```

Still need to think about the parenthesis and what we put in there. Maybe some
things are useless to see, unless you add something like `-l` for long format.
We will see.

Ok I see that the "official" way for `ls` is:

```
./tasks/20260330-202358/TASK.md:1: [PRIORITY: 100, TAGS: feature] Implement ls subcommand
```

Maybe we can exclude `[...]` and add it only if we use `-l` for long format.
For now let's just keep the official way.
