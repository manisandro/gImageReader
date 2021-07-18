#  הגדרות התקנת שפה של Tesseract

- ה*מנהל Tessdata*, זמין מהתפריט הנפתח ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/go-down-symbolic.symbolic.png) של הלחצן *זיהוי* ב*סרגל הכלים הראשי* מאפשר למשתמש לנהל את שפות הזיהוי הזמינות.

- להתקנת השפות באופן ידני:        

  - ב- **Linux**, זה מספיק להתקין את החבילה המתאימה להגדרת השפה הרצויה באמצעות יישום מנהל החבילה (ניתן לקרוא לחבילות משהו כמו *tesseract-langpack-<lang>*).

  - ב- **Windows**, ראשית, בדוק את הגרסה של gImageReader שבשימוש (ע"י כניסה ל- "על אודות").            

    - אם אתה משתמש ב- tesseract 4.x, גש אל https://github.com/tesseract-ocr/tessdata_fast.

    - אם אתה משתמש ב- tesseract 3.x והלאה, גש אל https://github.com/tesseract-ocr/tessdata.

      לאחמ"כ, בכפתור בחירת הענף, מתחת לתגיות, בחר את הגרסה שהיא שווה או מוקדמת יותר מגירסת ה- tesseract שבשימוש. לאחמ"כ הורד את הגדרות השפה הרצויה (*נתונים מאומנים יחד עם כל הקבצים המשלימים להם שפות מסוימות זקוקות) ולשמור אותם ב: התחל ← כל התוכניות ← gImageReader ← הגדרות שפת Tesseract.

- כדי לזהות מחדש את השפות הזמינות, ניתן להתחיל מחדש את התוכנית, או בחר *זיהוי שפות מחדש* ב*תפריט הנפתח של האפליקצייה*.
