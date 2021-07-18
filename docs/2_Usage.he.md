# שימוש

## ייבוא ופתיחת תמונות

- ניתן לפתוח / לייבא תמונות מהחלונית *תמונות*, שמופעלת ע"י לחיצה על הכפתור הימני העליון ב*סרגל הכלים הראשי*. ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/document-new-symbolic.symbolic.png)
- לפתיחת תמונה או מסמך PDF קיים, לחץ על כפתור הפתיחה בחלונית *תמונות* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/document-open-symbolic.symbolic.png).
- כדי לפתוח את כל התמונות בתיקייה, לחץ על כפתור בחירת תקייה בחלונית *תמונות* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/status/folder-open-symbolic.symbolic.png).
- ללכידת צילום מסך ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/devices/camera-photo-symbolic.symbolic.png).
- להדבקת נתוני תמונה מלוח הגזירים ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/edit-paste-symbolic.symbolic.png).
- לפתיחת קובץ שנפתח לאחרונה, לחץ על החץ לצד כפתור הפתיחה![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/go-down-symbolic.symbolic.png).
- באפשרותך לנהל את רשימת התמונות שנפתחו באמצעות הכפתורים שלצד כפתור הפתיחה ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/list-remove-symbolic.symbolic.png)   ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/places/user-trash-symbolic.symbolic.png)   ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/edit-clear-symbolic.symbolic.png). 
  קבצים זמניים (כגון צילומי מסך ונתוני לוח הגזירים) נמחקים אוטומטית כאשר התוכנית מופעלת.
- לסריקת תמונה ממכשיר סריקה, לחץ על הכרטיסייה *סריקה* בחלונית *מקור* ואז ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/devices/scanner-symbolic.symbolic.png).

## צפייה והתאמת תמונות

- השתמש בכפתורים ב*סרגל הכלים הראשי* כדי להתקרב ולהתרחק  ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/zoom-in-symbolic.symbolic.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/zoom-out-symbolic.symbolic.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/zoom-original-symbolic.symbolic.png)
  כמו גם לסובב את התמונה בזווית שרירותית ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/object-rotate-left-symbolic.symbolic.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/object-rotate-right-symbolic.symbolic.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/rotate_page.svg).
   ניתן גם לבצע זום ע"י גלילה על התמונה בזמן שמקש ה- CTRL לחוץ.
- כלים בסיסיים למניפולציות של התמונה ניתנים בסרגל הכלים *בקרות תמונה*, המופעל ע"י לחיצה על הכפתור *בקרות תמונה* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/controls.png) ב*סרגל הכלים הראשי*, הכלים המסופקים כרגע הם בהירות![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/brightness.png) וניגודיות ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/contrast.png) והתאמות, כמו גם התאמת רזולוציית התמונה ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/resolution.png) (through interpolation).
- ניתן לבחור מספר תמונות, כך שמתאפשר למשתמש לעבד מספר תמונות בבת אחת (ראה להלן).

## הכנות לקראת זיהוי

- כדי לבצע OCR (זיהוי תווים אופטי) בתמונה, המשתמש צריך לציין:        

  - תמונות הקלט (כגון תמונות לזיהוי),
  - מצב הזיהוי (*טקסט פשוט* או *hOCR, PDF*),
  - השפה(ות) לזיהוי ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/go-down-symbolic.symbolic.png).

- **תמונות הקלט** שנבחרו תואמות לערכים שנבחרו בחלונית *תמונות* בכרטיסייה *מקורות*. אם נבחרו מספר תמונות, התוכנית תתייחס לקבוצת התמונות כאל מסמך מרובה עמודים, ותשאל את המשתמש אילו עמודים יעבדו בתחילת הזיהוי.

- ניתן לבחור את מצב הזיהוי בתיבת הבחירה של מצב ה-OCR בסרגל הכלים הראשי:

  - המצב *טקסט פשוט* גורם למנוע ה-OCR לחלץ רק את הטקסט הרגיל ללא מידע על עיצוב ופריסה.
  - המצב *hOCR, PDF* גורם למנוע ה-OCR להחזיר את הטקסט שזוהה כמסמך *hOCR* HTML, הכולל מידע על העיצוב והפריסה של הדף שזוהה. *hOCR* הוא פורמט סטנדרטי לאחסון תוצאות זיהוי, וניתן להשתמש בו לשיתוף פעולה עם יישומים אחרים התומכים בתקן זה. gImageReader עובד עם מסמכי *hOCR* ויכול גם ליצור מסמך PDF לתוצאות הזיהוי.

- ניתן לבחור את **שפת הזיהוי** מהתפריט הנפתח ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/go-down-symbolic.symbolic.png) של *לחצן הזיהוי* ב*סרגל הכלים הראשי*. אם מותקן מילון איות להגדרת שפת ה-tesseract, ניתן לבחור בין דיאלקטים אזוריים זמינים של השפה. זה ישפיע רק על בדיקת האיות של שפת הטקסט שזוהה. הגדרות שפות tesseract לא מזוהות יופיעו לפי קידומת שם הקובץ שלהן, עם זאת, ניתן ללמד את התוכנית לזהות קבצים כאלה ע"י קביעת כללים מתאימים בתצורת התוכנית (ראה להלן).
 ניתן להגדיר שפות מרובות לזיהוי בפעם אחת מתפריט המשנה *רב לשוני* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/go-down-symbolic.symbolic.png)
 של התפריט הנפתח. ניתן לנהל את הגדרות שפות ה- tesseract המותקנות בתפריט *נהל שפות...* בתפריט הנפתח של הלחצן *זיהוי*,
 ראה גם ***הגדרות התקנת שפת ה- tesseract***.

## זיהוי ולאחמ"כ עיבוד - טקסט פשוט

- ניתן לבחור שטחים לזיהוי ע"י גרירה (לחצן שמאלי + העברת העכבר) סביב חלקי התמונה. ניתן גם לבחור מספר חלקים בתמונה ע"י לחיצה על CTRL בזמן הבחירה.
- לחלופין הלחצן *זיהוי פריסה אוטומטי*![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/autolayout.png) , הנגיש מ*סרגל הכלים הראשי*, ינסה להגדיר אוטומטית אזורי זיהוי מתאימים, כמו גם להתאים את סיבוב התמונה כפי הצורך.
- ניתן למחוק ולסדר מחדש את הבחירות באמצעות תפריט ההקשר המופיע בעת לחיצה ימנית עליהם. ניתן גם לשנות את גודל הבחירות הקיימות ע"י גרירת פינות מלבן הבחירה.
- ניתן לזהות את החלקים שנבחרו בתמונה (או את התמונה כולה, אם לא מוגדרות בחירות) ע"י לחיצה על הלחצן *זיהוי* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/insert-text-symbolic.symbolic.png) ב*סרגל הכלים הראשי*. לחלופין, ניתן לזהות אזורים בודדים ע"י לחיצה ימנית על בחירה. מתפריט ההקשר של הבחירה, ניתן להפנות את הטקסט שזוהה ללוח הגזירים, במקום אל חלונית הפלט.
- אם נבחרו מספר עמודים לזיהוי, התוכנית תאפשר למשתמש לבחור בין זיהוי האזור הנוכחי שנבחר ידנית - עבור כל הדפים, לבין ביצוע ניתוח פריסת עמודים לכל עמוד בנפרד, כדי לזהות אוטומטית אזורי זיהוי מתאימים.
- הטקסט שזוהה יופיע ב*חלונית הפלט*, שתופיע אוטומטית ברגע שטקסט כלשהו זוהה, (אלא אם כן הטקסט הועבר לזיהוי אל לוח הגזירים).
- אם זמין מילון איות לשפת הזיהוי, בדיקת איות אוטומטית תופעל על הטקסט שהופק. ניתן לשנות את מילון האיות שבשימוש בתפריט השפה ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/go-down-symbolic.symbolic.png) שלצד הלחצן *זיהוי* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/insert-text-symbolic.symbolic.png), או מהתפריט שיופיע בעת לחיצה ימנית על אזור הטקסט.
- כאשר טקסט נוסף זוהה, הוא יוכנס במיקום הסמן או יחליף את מאגר הטקסט הקיים, תלוי במצב שנבחר בתפריט *מצב הוספה*, אשר נמצא ב*סרגל הכלים של חלונית הפלט*.
- כלים אחרים לאחר עיבוד כוללים סגירת מעברי שורה, השמטת רווחים ועוד (זמין מהלחצן השני ב*סרגל הכלים של חלונית הפלט*), כמו גם חיפוש והחלפת טקסט. ניתן להגדיר רשימה של כללים לחיפוש והחלפה ע"י לחיצה על *חיפוש והחלפה* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/edit-find-replace-symbolic.symbolic.png) ב*סרגל הכלים של חלונית הפלט*, ואח"כ על הלחצן *החלפות*.
- ניתן לבטל שינויים במאגר הטקסט ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/edit-undo-symbolic.symbolic.png) ולבצע מחדש ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/edit-redo-symbolic.symbolic.png) ע"י לחיצה על הכפתורים המתאימים ב*סרגל הכלים של חלונית הפלט*.
- ניתן לשמור את תוכן מאגר הטקסט ע"י לחיצה על הלחצן *שמירה* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/document-save-as-symbolic.symbolic.png) ב*סרגל הכלים של חלונית הפלט*.

## זיהוי ולאחמ"כ עיבוד - hOCR, PDF

- במצב hOCR, תמיד הזיהוי הוא על כל הדפים של המקור(ות).

- תוצאות הזיהוי מוצגות ב*חלונית הפלט* כמבנה-עץ, המחולק לפי דפים, פסקאות, שורות טקסט, מילים וגרפיקה.

- כאשר נבחר ערך במבנה-העץ, האזור המתאים בתמונה יודגש. בנוסף, מאפייני העיצוב והפריסה של הערך מוצגים בלשונית *מאפיינים* מתחת לעץ המסמך. מקור ה- hOCR הגולמי מוצג בלשונית *מקור* מתחת לעץ המסמך.

- ניתן לערוך את טקסט המילה בעץ המסמך ע"י לחיצה כפולה על ערך המילה המתאים. אם מילה מסוימת הינה בעלת איות-שגוי, היא תופיע באדום. לחיצה ימנית על מילה בעץ המסמך תציג תפריט הצעות איות.

- ניתן לשנות מאפיינים עבור ערך שנבחר ע"י לחיצה כפולה על ערך המאפיין הרצוי בלשונית *מאפיינים*. פעולות מעניינות לערכי טקסט הם שינוי האזור התוחם, שינוי השפה ושינוי גודל הגופן. מאפיין השפה מגדיר גם את שפת האיות המשמשת לבדיקת המילה שנבחרה. ניתן לערוך את האזור התוחם ע"י שינוי גודל מלבן הבחירה ע"ג התמונה.

- ניתן למזג פריטי מילים סמוכים ע"י לחיצה ימנית על הפריטים המתאימים שנבחרו.

- ניתן להסיר כל פריט מהמסמך ע"י לחיצה-ימנית על הפריט המתאים.

- ניתן להגדיר אזורים גרפיים חדשים ע"י בחירת הערך *הוסף אזור גרפי* בתפריט ההקשר של הפריט לעמוד המתאים וציור מלבן על התמונה.

- עץ המסמך ניתן לשמירה כ*מסמך hOCR HTML* דרך הלחצן *שמור כטקסט hOCR* ב*סרגל הכלים של חלונית הפלט*. ניתן לייבא מסמכים קיימים ע"י לחיצה על *פתח קובץ hOCR* ב*סרגל הכלים של חלונית הפלט*.

- ניתן ליצור מסמכי PDF מתפריט הייצוא של PDF ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/16x16/actions/document-export.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/16x16/mimetypes/application-pdf.png) בסרגל הכלים של חלונית הפלט. שני מצבים זמינים:

 - *PDF* תיצור מסמך PDF משוחזר באותה פריסה דומה וגרפיקה / תמונות כמו מסמך המקור.

 - *מסמך PDF עם שכבת טקסט בלתי נראה* תיצור מסמך PDF עם תמונת המקור שלא השתנתה כרקע, ושכבת טקסט בלתי נראה (אך ניתן לבחירה) מעל טקסט המקור המתאים בתמונה. מצב ייצוא זה שימושי ליצירת מסמך זהה מבחינה ויזואלית לתמונה שנקלטה, ועם זאת עם טקסט הניתן לחיפוש ובחירה.

- בעת הייצוא ל- PDF, המשתמש יתבקש להשתמש במשפחת הגופנים, והאם לכבד את גדלי הגופנים שזוהו ע"י מנוע ה- OCR, והאם לנסות לאחד את ריווח שורות הטקסט. כמו כן, המשתמש יכול לבחור את תבנית הצבע, הרזולוציה ודחיסת התמונה לשימוש בתמונות במסמכי PDF לשליטה בגודל הפלט שנוצר.
