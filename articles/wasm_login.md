---
layout: default
---

# Task - 1: login.c

[Before going trying the tasks, please read this, it will help a lot.](https://www.usenix.org/system/files/sec20-lehmann.pdf)

Sidenote: The tasks are all compiled with emscripten, which takes input in a popup window before the task starts. After sending in your input, it will pop up again until you press cancel. To use it correctly, you'll need to enter your input in the first popup, and cancel the second one.

Your task here is to get the `Welcome Admin!` message

[Open Task - 1 login.c](../assets/wasm/login.html)

<details>
  <summary>Hint 1</summary>

This is a simple buffer overflow, much like in a normal binary.
</details>

<details>
  <summary>Hint 2</summary>

Try to overwrite the `user` variable on the stack to pass the check.
</details>

<details>
  <summary>Hint 3</summary>

Man... what else is there to say?
</details>
