---
layout: default
---

# Task - 2: register.c

[Before going trying the tasks, please read this, it will help a lot.](https://www.usenix.org/system/files/sec20-lehmann.pdf)

Sidenote: The tasks are all compiled with emscripten, which takes input in a popup window before the task starts. After sending in your input, it will pop up again until you press cancel. To use it correctly, you'll need to enter your input in the first popup, and cancel the second one.

Your task here is to change the isAdmin parameter from `false` to `true` when it is printed.

[Open Task - 2 register.c](../assets/wasm/register.html)

<details>
  <summary>Hint 1</summary>

Do you know the difference between a stack-based buffer overflow, and a stack overflow?
</details>

<details>
  <summary>Hint 2</summary>

In web assembly, there is no write protected memory space, not even constant strings are safe.
</details>

<details>
  <summary>Hint 3</summary>

Repeat the recursion until the stack is over the constant data region, then overwrite the constant strings stored there.
</details>
