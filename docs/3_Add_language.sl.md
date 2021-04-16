#  Tesseract jezikovne definicije

- *Upravljavnik Tessdata*, ki ga odprete z klikom n *Upravljaj z jeziki...* na dnu spustnega seznama ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) omogoča uporabniku upravljanje z jeziki za prepoznavo. Za dodat jezik, le obkjukajte želen jezik in upravljalnik bo samodejno prenesel potrebne datoteke. 

- Če želite namestiti jezike popolnoma ročno:

  - **Linux**, v vašem upravitelju paketov poiščite in namestite ustezno jezikovno definicijo (paket se bo morebiti imenoval *tesseract-langpack-<jezik>*).

  - **Windows**, najprej v gImageReader odprite ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/categories/preferences-system.png) ter kliknite na ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/help-about.png) *O programu* da preverite katero verzija tesseract je v uporabi.            

    - Če se uporablja tesseract 4.x pojdite na https://github.com/tesseract-ocr/tessdata_fast.

    - Če se uporablja tesseract 3.x ali starješi pojdite na https://github.com/tesseract-ocr/tessdata.

      Nato v seznamu poiščite verzijo ki je manjša ali enaka vaši tesseract verzji. Nato prenesite želeni jezik (*.traineddata in pripadajče dopolnilne datoteke, ki jih nekateri jeziki potrebujejo) in jh shranite v *Start→All Programs→gImageReader→Tesseract language definitions*.

- Da sprožite ponovno zanavo jezikov na voljo, lahko ponovno zažene program ali ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/categories/preferences-system.png)→ ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/view-refresh.png)*Ponovno zaznaj jezike*