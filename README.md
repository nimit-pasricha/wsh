# wsh: Custom POSIX Shell in C

wsh ("woosh") is a custom Unix shell written in C that implements core operating system features from the ground up, including process management, command execution, and inter-process communication.


## ✨ Features

wsh is designed to be a lightweight yet functional shell, supporting a range of essential features that users expect from a modern command-line interface.

* **Interactive and Batch Modes**: Run wsh as an interactive prompt or execute commands from a script file.
* **Command Execution**: Executes external commands by searching the `PATH` environment variable.
* **Piping**: Chains multiple commands together using the `|` operator, allowing for complex data processing pipelines.
* **Built-in Commands**: A robust set of internal commands that are handled directly by the shell without creating new processes.
    * `exit`: Terminates the shell session.
    * `cd [path]`: Changes the current working directory. If no path is given, it changes to the `HOME` directory.
    * `path [new_path]`: Displays or modifies the `PATH` environment variable.
    * `alias [name = value]`: Creates or lists command aliases. Supports multi-level substitution and detects circular dependencies.
    * `unalias <name>`: Removes a previously defined alias.
    * `which <command>`: Shows whether a command is a built-in, an alias, or an external executable.
    * `history [n]`: Displays the command history or executes the nth command from history.

---

## 🚀 Getting Started

Follow these instructions to build and run wsh on your local machine.

### Prerequisites

You'll need a C compiler (like `gcc` or `clang`) and `make` installed on your system.

### Building the Shell

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/nimit-pasricha/wsh.git
    cd wsh/src/
    ```

2.  **Compile the source code**:
    The project includes several source files for the shell logic and its underlying data structures. You can compile them all with the following command:
    ```bash
    make wsh
    ```

### Running wsh

* **Interactive Mode**: Launch the shell and get a prompt.
    ```bash
    ./wsh
    ```

* **Batch Mode**: Execute a file containing a series of shell commands.
    ```bash
    ./wsh your_script.sh
    ```

---

## 💻 Usage and Examples

Here are some examples of what you can do with wsh.

* **Execute an external command:**
    ```
    wsh> ls -l /tmp
    ```

* **Use pipes to combine commands:**
    ```
    wsh> cat wsh.c | grep "fork" | wc -l
    ```

* **Create and use an alias:**
    ```
    wsh> alias ll = 'ls -alF'
    wsh> ll
    ```

* **Modify the PATH:**
    ```
    wsh> path /bin:/usr/bin:/usr/local/bin
    ```

---

## 🛠️ Code Structure

The shell's logic is primarily contained within `wsh.c` and is organized as follows:

* **`main()`**: The entry point that determines whether to run in interactive or batch mode.
* **`interactive_main()` & `batch_main()`**: The main loops for handling user input or reading from a script file.
* **`parseline_no_subst()`**: A robust parser that splits a command line string into an array of arguments, respecting single-quoted strings.
* **`execute_builtin()`**: A dispatcher that checks if a command is a built-in and, if so, calls the appropriate handler function (e.g., `change_directory()`, `create_alias()`).
* **`get_command_path()`**: A utility function that searches the directories listed in the `PATH` environment variable to find an executable.
* **Data Structures**: The shell leverages a custom **`HashMap`** for managing aliases and a **`DynamicArray`** for storing command history, demonstrating efficient data management in C.

---

## 🚧 TODO & Future Work

This project is under active development. Here is the roadmap for planned features:

-   [ ] **Advanced Terminal I/O**
    -   [ ] Implement raw/canonical mode switching to capture individual keystrokes.
    -   [ ] Add support for the **Up** and **Down** arrow keys to navigate through command history.
    -   [ ] Implement a reverse history search triggered by `Ctrl+R`.

-   [ ] **Shell Variables**
    -   [ ] Add support for setting and unsetting local variables (e.g., `X='hello world'`).
    -   [ ] Implement variable substitution in commands (e.g., `echo $MYVAR`).

-   [ ] **Advanced Shell Features**
    -   [ ] I/O Redirection (`>`, `>>`, `<`).
    -   [ ] Background Processes (`&`).
    -   [ ] Job Control (`jobs`, `fg`, `bg`).
    -   [ ] Conditional Execution (`&&`, `||`).
    -   [ ] Tab Completion for commands and file paths.
    -   [ ] Globbing / Wildcard Expansion (`*`, `?`).
