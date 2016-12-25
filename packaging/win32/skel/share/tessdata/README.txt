This folder contains tesseract language definitions.

To add additional language definitions:
- Use the tessdata manager from the language selection menu in gImageReader
- Or install the languages manually:
    * In the gImageReader about dialog, check which version of tesseract is used
    * Go to the https://github.com/tesseract-ocr/tessdata, and in the branch selection button, under tags, select the version which is *less or equal* the tesseract version in use
    * Download the desired language definitions (*.traineddata along with any supplementary files which certain languages need) and save them inside this folder
    * If gImageReader is running, select "Redetect Languages" from the application menu, or restart the application
