This folder contains tesseract language definitions.

To add additional language definitions:
- Use the tessdata manager from the language selection menu in gImageReader
- Or install the languages manually:
    * In the gImageReader about dialog, check which version of tesseract is used
    * If using tesseract 4.x, go to https://github.com/tesseract-ocr/tessdata_fast
    * If using tesseract 3.x or older, go to https://github.com/tesseract-ocr/tessdata
    * In the branch selection button, under tags, select the version which is *less or equal* the tesseract version in use
    * Download the desired language definitions (*.traineddata along with any supplementary files which certain languages need) and save them inside this folder
    * If gImageReader is running, select "Redetect Languages" from the application menu, or restart the application
