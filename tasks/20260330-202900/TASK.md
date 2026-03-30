# Implement nvim plugin to create task and link from editor

- STATUS: OPEN
- PRIORITY: 90
- TAGS: feature

This is a follow up to the Task Tracker project. We want to be able to create
tasks from the editor and link them to the file and line number where they were
created. This will allow us to easily jump to the task from the editor and see
the details of the task.

In code the tasks will look like this:

```
TODO(20260330-202900): Implement nvim plugin to create task and link from editor
```

I would like to be able to have a keybinding that creates a `huid` and a `task`
and links them together. Then whatever I type in the TODO comment will be the
title of the task.

Or I guess the workflow can be like this:
- I write
```
TODO: Implement nvim plugin to create task and link from editor
```
- I put the cursor on the TODO and press the keybinding to create the task and link
- The plugin creates a `huid` and a `task` and links them together.
