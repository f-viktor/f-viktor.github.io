---
layout: default
---

# Task - 3: heap.c

[Before going trying the tasks, please read this, it will help a lot.](https://www.usenix.org/system/files/sec20-lehmann.pdf)

Sidenote: The tasks are all compiled with emscripten, which takes input in a popup window before the task starts. After sending in your input, it will pop up again until you press cancel. To use it correctly, you'll need to enter your input in the first popup, and cancel the second one.

Your task is to get the `Wow, you're cool!` welcome message. This task is somewhat more complicated than the previous two.

[Open Task - 3 heap.c](../assets/wasm/heap.html)

<details>
  <summary>Hint 1</summary>

Function pointers in web assembly are indexes to a table, the first function is indexed 1, the second 2 etc...
</details>

<details>
  <summary>Hint 2</summary>

Read up on the unlink exploit.
</details>

<details>
  <summary>Hint 3</summary>

Create a dummy free heap object in the memory allocated for the `two` variable, use it to overwrite the function pointer on the stack upon free().
</details>
