# Program options

- The program options can be accessed from the *application menu* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/categories/preferences-system.png), which opens when clicking the right-most button of the *main toolbar*. When running the application within the Gnome 3 desktop environment,  the application menu is part of the top bar of the desktop shell.

- Options allow setting the font of the output pane, as well as  determining whether the application will notify about missing spelling  dictionaries and new program versions.

- When running the Gtk+ interface, the options also allow setting the orientation of the *output pane* (if vertical, it will occupy the right portion of the application, if  horizontal, it will occupy the lower portion). When running the Qt  interface, the position of the output pane can be freely moved around by dragging on the title bar of the output pane.

- The *language data location* setting allows to control whether tesseract language definitions and spelling dictionaries are saved in system-wide (i.e. `%ProgramFiles%` under Windows or typically below `/usr` on Linux) or user-local (i.e. below the current user's home directory)  directories. This is useful if the user does not have writing privileges in system-wide locations.

- Additionally, one can define new rules to match tesseract  language definitions to a language (unfortunately, the tesseract  language definitions do not include this information). The list of  predefined rules can be seen in the Predefined language definitions section. Additional definitions can be added clicking on the Add button below. The rules for a language definition, which consists of three fields, are as follows:

***Filename prefix***: The filename of tesseract language data files is of the format *<prefix>.traineddata*, i.e. for English, the file is called *eng.traineddata* and the prefix is *eng*.

***ISO code***: This is the [ISO 639-1 language code](http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes) (i.e. *en*), optionally combined by an underscore with the [ISO 3166-2 country code](http://en.wikipedia.org/wiki/ISO_3166-2) (i.e. *en_US*). This information is necessary to match spelling dictionaries to the  language. The choice of the actual country code is not strictly  relevant, but it is necessary for the automatic installation of spelling dictionaries to find a relevant package of dictionaries. This code can  also be made up if no appropriate choices exist, the only result being  that no relevant spelling-dictionaries will be matched with the  language.

***Native name***: The native name of the language simply determines the label of the entry for the language in the *language menu*.