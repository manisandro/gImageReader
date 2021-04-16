# Contributing

For suggestions and contributions of any kind, please file tickets and/or pull-requests on the [GitHub project page](https://github.com/manisandro/gImageReader), or contact me at [manisandro@gmail.com](mailto:manisandro@gmail.com). I'd especially appreciate translations - here are the main steps for creating a translation:

A) gImageReader translations are hosted on [weblate](https://hosted.weblate.org/projects/gimagereader/) and are periodically merged into master.

or

B):

1. Download the [latest source archive](https://github.com/manisandro/gImageReader/archive/master.zip).
2. Enter the `po` folder.
3. To create a new translation, copy the `gimagereader.pot` file to `<language>.po` (i.e. `de.po` for German). To edit an existing translation, simply pick the corresponding file.
4. Translate the strings in `<language>.po`.
5. Send the **po** file to [manisandro@gmail.com](mailto:manisandro@gmail.com). Thanks!

## Debugging and support

If you find an issue or have a suggestion, please file a ticket to the [gImageReader issue tracker](https://github.com/manisandro/gImageReader/issues), or contact me directly at [manisandro@gmail.com](mailto:manisandro@gmail.com). Be sure to also consult the [FAQ](https://github.com/manisandro/gImageReader/wiki/FAQ). If you are experiencing crashes or hangs, please also try to include the following information in the ticket/email:

- If the crash handler appears, include the backtrace which is shown  there. To make sure that the backtrace is complete, if you are running  the application under Linux, make sure that the `gdb` debugger as well as the debugging symbols are installed if your distribution  provides them. The package containing the debugging symbols is usually  called **<packagename>-debuginfo** or **<packagename>-dbg**. If you are running the application under Windows, some debugging symbols are installed by default.
- If you are running under Windows, include the `%ProgramFiles%\gImageReader\gimagereader.log` and `%ProgramFiles%\gImageReader\twain.log` log files.
- If you are running under Linux, run the application from a terminal and include any output which appears on the terminal.
- Try to describe as best as you can what you were doing and whether the problem is reproducible.