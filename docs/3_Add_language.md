#  Install Tesseract language definitions

- The *Tessdata manager*, available from the drop-down menu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) of the *recognize button* in the *main toolbar* allows the user to manage the available recognition languages.

- To install the languages manually:        

  - On **Linux**, it's sufficient to install the package  corresponding to the language definition one wants to install via the  package management application (the packages may be called something  like *tesseract-langpack-<lang>*).

  - On Windows, first, in the gImageReader about dialog, check which version of tesseract is used.            

    - If using tesseract 4.x, go to https://github.com/tesseract-ocr/tessdata_fast.

    - If using tesseract 3.x or older, go to https://github.com/tesseract-ocr/tessdata.

      Then, in the branch selection button, under tags, select the version which is less or equal

     the tesseract version in use. Then download the desired language  definitions (*.traineddata along with any supplementary files which  certain languages need) and save them to 

    Start→All Programs→gImageReader→Tesseract language definitions.

- To re-detect the available languages, one can restart the program, or select *Redetect Languages* from the *application menu*.