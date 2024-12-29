# User-Mode-Thread-Library

The main deliveraable for this project is a basic thread system for Linux.

In this asssignment, we were tasked with implementing an API for three pthread functions, as well as implicitly developing a scheduler.

For this assignment, I found the hardest parts to be properly allocating memory to my stack, as well as figuring out how to conceptually understand a multithreaded program.

I also dealt with some difficulty in freeing my stack memory, as I would free it while still in the thread's context.

I needded to develop a scheduler and had to prepare it for switching into a multithreaded program, this was done with a global variable and with it an initalizer.
