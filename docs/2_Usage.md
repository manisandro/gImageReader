# Usage

## Opening and importing images

- Images can be opened/imported from the *sources pane*, which is activated by clicking on the top-left button in the *main toolbar*. ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-image.png)
- To open an existing image or PDF document, click on the open button in the *images tab* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-open.png).
- To open all images in one directory, click the open directory button in the *images tab* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/folder-open.png).
- To capture a screenshot ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/devices/camera-photo.png).
- To paste image data from the clipboard ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-paste.png).
- To open a recently opened file, click on the arrow next to the open  button![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/pan-down-symbolic.symbolic.png).
- You can manage the list of opened images with the buttons next  to the open button ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/list-remove.png)   ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/places/user-trash.png)   ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-clear.png). 
  Temporary files (such as screenshots and clipboard  data) are automatically deleted when the program exists.
- To acquire an image from a scanning device, click on the *acquire tab* in the *sources pane* and then ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/devices/scanner.png).

## Viewing and adjusting images

- Use the buttons in the *main toolbar* to zoom in and out  ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/zoom-in.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/zoom-out.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/zoom-original.png)
  as well as rotate the image by an arbitrary angle![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/object-rotate-left.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/object-rotate-right.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/rotate_page.svg).
   Zooming can also be  performed by scrolling on the image with the CTRL key pressed.
- Basic image manipulation tools are provided in the *image controls toolbar*, which is activated by clicking on the *image controls button* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/controls.png) in the *main toolbar*. The provided tools currently allow brightness![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/brightness.png) and contrast ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/contrast.png) adjustments  as well as adjusting the image resolution ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/resolution.png) (through interpolation).
- Multiple images can be selected, which allowes the user to process multiple images in one go (see below).

## Preparing for recognition

- To perform OCR on an image, the user needs to specify:        

  - The input images (e.g. images to recognize),
  - The recognition mode (e.g. *plain text* vs *hOCR, PDF*)
  - The recognition language(s) ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png).

- The **input images** correspond to the selected entries in the *images tab* in the *sources pane*. If multiple images are selected, the program will treat the set of  images as multipage document and ask the user which pages to process  when recognition is started.

- The recognition mode can be selected in the OCR mode combobox in the main toolbar:

  - The *plain text* mode makes the OCR engine extract only the plain text, without formatting and layout information.
  - The *hOCR, PDF* mode makes the OCR engine return the recognized text as a *hOCR* html document, which includes formatting and layout information for the recognized page. *hOCR* is a standard format for storing recognition results and can be used to interoperate with other application supporting this standard.  gImageReader can process *hOCR* documents further to generate a PDF document for the recognition result.

- The **recognition language** can be selected from the drop-down menu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) of the *recognize button* in the *main toolbar*. If a spelling dictionary is installed for a tesseract language  definition, it is possible to choose between available regional flavors  of the language. This will only affect the language for spell-checking  the recognized text. Unrecognized tesseract language definitions will  appear by their filename prefix, one can however teach the program to  recognize such files by defining appropriate rules in the program  configuration (see below). Multiple recognition languages can be  specified at once from the *Multilingual* submenu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) of the drop-down menu. The installed tesseract language definitions can be managed from the *Manage languages...* menu entry in the drop-down menu of the *recognize button*, see also ***Installation of tesseract language definitions***.

## Recognizing and post-processing - plain text

- Areas to be recognized can be selected by dragging (left click + mouse move) a rectangular area around portions of the image. Multiple  selections are possible by pressing the CTRL key while selecting.
- Alternatively, the *automatic layout detection button*![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/autolayout.png) , accessible from the *main toolbar* will attempt to automatically define appropriate recognition areas, as well as adjust the rotation of the image if necessary.
- Selections can be deleted and reordered via the context menu  which appears when right-clicking on them. It is also possible to resize existing selections by dragging the corners of the selection rectangle.
- The selected portions of the image (or the entire image, if no selections are defined) can be recognized by pressing on the *recognize button* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-text.png) in the *main toolbar*. Alternatively, individual areas can be recognized by right-clicking a  selection. From the selection context menu, it is also possible to  redirect the recognized text to the clipboard, instead of the output  pane.
- If multiple pages are selected for recognition, the program  allows the user to choose between either recognizing the current manually selected area for each individual page, or performing a  page-layout analysis on each page to automatically detect appropriate  recognition areas.
- Recognized text will appear in the *output pane* (unless the text was redirected to the clipboard), which is shown automatically as soon as some text was recognized.
- If a spelling dictionary for the recognition language is  available, automatic spell-checking will be enabled for the outputted  text. The used spelling dictionary can be changed either from the  language menu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) next to the *recognize button* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-text.png), or from the menu which appears when right-clicking in the text area.
- When additional text is recognized, it will either get appended, inserted at cursor position, or replace the previous content of the  text buffer, depending on the mode selected in the *append mode menu*, which can be found in the *output pane toolbar*.
- Other post-processing tools include stripping line breaks, collapsing spaces and more (available from the second button in the *output pane toolbar*), as well as searching and replacing text. A list of search and replace rules can be defined by clicking on the *Find and replace* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-find-replace.png)button in the *output pane toolbar* and then clicking on the *Substitutions* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-edit.png) button.
- Changes to the text buffer can be undone ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-undo.png) and redone ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-redo.png) by clicking on the appropriate buttons in the *output pane toolbar*.
- The contents of the text buffer can be saved to a file by clicking on the *save button* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-save-as.png) in the *output pane toolbar*.

## Recognizing and post-processing - hOCR, PDF

- In hOCR mode, always the entire page of the selected source(s) is recognized.

- The recognition result is presented in the *output pane* as a tree-structure, divided in pages, paragraphs, textlines, words and graphics.

- When an entry in the tree-structure is selected, the  corresponding area is highlighted in the image. Additionally, formatting and layout properties of the entry are shown in the *Properties* tab below the document tree. The raw hOCR source is visible in the *Source* tab below the document tree.

- The word text in the document tree can be edited by  double-clicking the respective word entry. If a word is mis-spelled, it  will be rendered red. Right-clicking a word in the document tree will  show a menu with spelling suggestions.

- Properties for a selected entry can be modified by double clicking the desired property value in the *Properties* tab. Interesting actions for text entries are tweaking the bounding  area, changing the language and modifying the font size. The language  property also definies the spelling language used to check the  respective word. The bounding area can also be edited by resizing the  selection rectangle in the canvas.

- Adjacent word items can be merged by rightclicking the respective selected items.

- Arbitrary items can be removed from the document via right-click on the respective item.

- New graphic areas can be defined by selecting the *Add graphic region* entry of the context menu of the respective page item and drawing a rectangle on the canvas.

- The document tree can be saved as a *hOCR HTML document* via the *Save as hOCR text* button in the *output pane toolbar*. Existing documents can be imported via the *Open hOCR file* button in the *output pane toolbar*.

- PDF files can be generated from the PDF export menu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-export.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/mimetypes/application-pdf.png) in the output pane toolbar. Two modes are available:

  - *PDF* will generate a reconstructed PDF the same layout and graphics/pictures as the source document.
  - *PDF with invisible text overlay* will generate a  PDF with the unmodified source image as background and invisible (but  selectable) text overlaid above the respective source text in the image. This export mode is useful for generating a document which is visually  identical to the input, but with searchable and selectable text.

- When exporting to PDF, the user is prompted for the font family  to use, whether to honour the font sizes detected by the OCR engine, and whether to attempt to homogenize the text line spacing. Also, the user  can select the color format, resolution and compression method to use  for images in the PDF document to control the size of the generated  output.
