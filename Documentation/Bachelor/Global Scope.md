
# 4. Von Lexical to Global Scope mittels Variable Reuse und Parameter Promotion
- **Was is lexical Scope?**
- **Was ist global Scope?**
- **Idee zur Transformation**:
    - einfach alle lokalen Variablen in globale Variablen umwandeln?
    - zu einfach, variable shadowing, name clashing
- **Herausforderungen bei KSP**:
    - KSP hat eine maximal Anzahl an statements im on init callback und Variablen verbrauchen Speicher
      - Ziel: Minimierung der Variablenanzahl und Speicherverbrauch    
    - Race Conditions bei asynchronen Callbacks.
    - Race Conditions bei Wiedereintritt
      - Ziel: Kennzeichnen von thread-unsicheren Callbacks, dimension inflation bei asynchronen Callbacks
    - 
- **Vorgehen**:
    - Detaillierte Beschreibung der Scope- und Variablenverwaltung.
    - Implementierung von Parameter Promotion und Lambda Lifting.
- **Herausforderungen**:
    - Optimierung der Variablenwiederverwendung.
    - Umgang mit asynchronen Operationen und Race Conditions.
    - Umgang mit Wiedereintritt in Callback?
- **Lösungen**: 
    - Array Assignment Lowering
    - Konvertierung von Array-Initialisierungen in Iterationen.
    - Herausforderungen:
        - Mangel an direkter Array-Unterstützung in KSP.
        - Performance-Einbußen bei großen Arrays.


Deine Struktur macht bereits Sinn, aber ich würde die Kapitelreihenfolge leicht anpassen und stärker auf den praktischen Ablauf deiner Arbeit eingehen. Ziel ist es, die **Problemstellung**, deine **Herangehensweise** (Algorithmen), die **Herausforderungen** und deine **Lösungen** systematisch darzustellen.

---

# 4. Von Lexical to Global Scope mittels Variable Reuse und Parameter Promotion  

1. **Einleitung**  
   - **Was ist Lexical Scope?**  
     Erkläre kurz, dass Variablen in CKSP lokal an ihren Gültigkeitsbereich gebunden sind und nur in diesem sichtbar sind.  
   - **Was ist Global Scope?**  
     Variablen im Global Scope sind während der gesamten Programmlaufzeit sichtbar, was in KSP notwendig ist, da KSP keine Lexical Scopes unterstützt.  
   - **Warum ist eine Transformation notwendig?**  
     - Vorteile von Lexical Scope:  
       - Bessere Code-Organisation und -Lesbarkeit.
       - Wird fast überall in modernen Programmiersprachen verwendet.
       - Deswegen nutzt CKSP Lexical Scope für saubere Trennung von Variablen.  

2. **Grundidee zur Transformation**  
   - **Naiver Ansatz**: Einfach alle lokalen Variablen zu globalen umwandeln.  
     - **Problem**: Variable Shadowing und Name Clashing.  
     - **Lösung**: Variablenumbenennung mittels Gensym, um eindeutige Namen zu garantieren.  
     - **Aber**: Dieser Ansatz führt zu ineffizientem Code.
     - **Außerdem**:
       - KSP begrenzt die Anzahl an Deklarationen im `on init`-Callback.  
       - Mehr Speicherverbrauch durch mehr Variablen.
       - **Ziel**: Optimierung der Variablenwiederverwendung und Reduktion des Speicherverbrauchs.  
   - **Speicher- und Performance-Optimierung**:  
     - Minimierung der Variablenanzahl durch das Wiederverwenden passiver Variablen.
   <!-- - **Was it mit Funktionen?**
     - KSP unterstützt keine Funktionen mit Parametern und Rückgabewerten, deshalb müssen auch diese Variablen in den on init callback verschoben werden.
     - Parameter Promotion: Lokale Variablen werden in Funktionsparameter umgewandelt. -->

3. **Herausforderungen durch KSP und seine Limitierungen**  
   - **Asynchrone Operationen**:  
     - Race Conditions durch `wait` und andere asynchrone Funktionen.  
       - **Lösung**: Kennzeichnung von nicht-thread-sicheren Callbacks und Funktionen.  
   - **Wiedereintritt (Reentrancy)**:  
     - Callbacks können erneut aufgerufen werden, bevor der vorherige abgeschlossen ist.  
       - **Ziel**: Sicherstellen der Konsistenz der Variablenzustände.  

4. **Vorgehen und Algorithmen zur Transformation**  
   - **Variablen- und Scope-Tracking**:  
     - **Stack-basierte Scope-Verwaltung**: Ein Stack aus Maps speichert Variablen in verschiedenen Scopes.  
     - Fungiert auch gleichzeitig als VariableChecking, um zu schauen dass alle Variablen korrekt deklariert wurden und die Referenzen intern durch Pointer mit ihren Deklarationen zu verbinden.
     - NodeBlock: besteht aus verschiedenen Statements und kann ein Scope sein.
     - Eine separater AST Durchlauf bestimmt welcher Block ein Scope sein soll durch bestimmte Kriterien. Also zb wenn das Parent ein Callback, eine Funktionsdefinition, ein if-statement oder eine Schleife ist.
     - Im anschließenden VariableChecking Pass, wird in einer separaten Datenstruktur ein Stack aus Maps initiiert. Dieser erhält eine neue Map, wenn eine NodeBlock betreten wird, in die alle dort deklarierten Variablen geschrieben werden, und wird wieder entfernt, wenn der Block verlassen wird.
     - Besonderheit sind Funktionsdefinitionen, bei denen die Parameterdeklarationen mit in die Map des Bodys kopiert werden, damit diese sich im gleichen Scope wie die lokalen Variablen befinden.
     - jedes deklaration statement wird nun analysiert, ob es sich um ein lokales handelt oder nicht:
       - wenn es sich im on init callback befindet, ist es per default global und wird in die unterste Map des Stacks geschrieben
       - allerdings kann es durch die extra Verwendung von 'local' in der Deklaration auch als lokal im on init callback markiert werden
       - ansonsten sind alle deklarationen per default lokal und werden in die oberste Map des Stacks geschrieben (außer wenn sie extra mit 'global' markiert wurden)
       - Alle Variablen in diesen Deklarationen werden in die entsprechende Map geschrieben.
       - Zur gleichen Zeit werden die Namen dieser Variablen auch in eine globale Gensym instanz geschrieben, um später Kollisionen zu vermeiden und garantiert eindeutige Namen generieren zu können.
       - Das kommt bereits hier zum Einsatz, denn extra 'global' markierte Variablen werden in der globalen Map gespeichert und erhalten bei selten aufkommenden NAme Clashes einen eindeutigen Namen.
     - Bei Referenzen wird nun geprüft, ob die Variable bereits in einer der Maps des Stacks existiert. Falls ja, wird die Referenz durch einen Pointer auf die Deklaration ersetzt. Falls nein, wird eine Fehlermeldung über eine nicht deklarierte Variable ausgegeben.
       - Diese Pointer werden später für diverse Dinge benötigt
         - eindeutige Zuordnung von womöglich gleichnamigen Variablen und ihren Referenzen in verschiedenen Scopes
         - Verschiedene Lowering Prozesse von Referenzen, die Informationen aus den Deklarationen benötigen (size konstanten bei multidimensionalen Arrays, Typen von Variablen, etc.)
         - unused variablen können so auch leichter erkannt werden, indem keine referenz auf sie zeigt -> Optimization Passes
     - Damit endet der VariableChecking Pass und auch die Konstruktion des Lexical Scopings.
    
   - **Register Reuse**:
      - ist ein Optimierungsprozess im Compilerbau, dessen Ziel Speicherplatz zu sparen und die Laufzeit zu verbessern ist.
      - Passiert eigentlich sehr low level, indem Register in Assembler Code wiederverwendet werden, um weniger Speicherzugriffe zu benötigen.
      - Aber der name passt hier auch gut, weil es darum geht, Variablen in KSP wiederverwendbar zu machen.

      - Da KSP einen garantiert linearen Kontrollfluss innerhalb der Callbacks hat, müssen bei einer Transformation nicht mehrere Callbacks gleichzeitig betrachtet werden. 
      - Für die grundlegende Erklärung des Register Reuse Algorithmus sollen thread-unsafe Callbacks und Funktionen zunächst außen vor bleiben und wir nehmen an, dass Callbacks sequenziell abgearbeitet werden.
      - Ziel des Register Reuse Algos ist es, passive Variablen zu sammeln und diese wiederzuverwenden, um die Anzahl an Deklarationen zu reduzieren und somit Speicherplatz zu sparen.
      - Passive Variablen sind Variablen, die in einem lokalen Scope deklariert wurden und durch die Natur des Lexical Scopings nach dem Verlassen dieses Scopes nicht mehr verwendet werden können, ihr dynamic extend also abgelaufen ist.

      Algorithmus:
      1. Der Algorithmus benutzt die Visitor Pattern Implementierung des ASTs, um alle Nodes zu besuchen.
      2. Sobald eine NodeBlock Node mit einem Scope betreten wird, wird eine neue Map erstellt, die Namen und Pointer zu alle lokalen Variablen, die in diesem Scope deklariert wurden und die über das Visitor Pattern besucht wurden, speichert.
      3. Anschließend wird bei jedem Besuch eines Deklarationsstatements, die Variable – sofern sie nicht global ist – in die Map des aktuellen Scopes geschrieben. Ist die Variable eine Konstante, so kann sie nicht wiederverwendet werden, da damit reassignments einhergehen. In diesem Fall wird sie zu den Globalen Deklarationen hinzugefügt, um später mit Gensym umbenannt zu werden.
      4. Beim Verlassen der NodeBlock Node (und des Scopes) werden die gesammelten Variablen in eine neue, globale Map hinzugefügt, die alle passiven Variablen enthält (Beispiel). Hier kann bereits variable name clashing auftreten (Beispiel), da Variablen aus verschiedenen Scopes in der gleichen Map landen. Daher besitzt die Map pro Hashvalue einen Vector an NodeDatastructures Nodes.

        ```cksp
            if(i = NUM_GROUPS)
                declare j: int
                message("If-Variable j: " & j)
            else if(i < NUM_GROUPS)
                declare j: int
                message("Else-If-Variable j: " & j)
            else if(i > NUM_GROUPS)
                declare j[5]: int[] := (0)
                message("Else-Array j: " & j[0])
            end if
        ```

      Die function für die (string) Hashwerte verarbeitet dabei folgendes:
            - Variablenname
            - Typ
            - Array Größe
      5. Sobald nun eine lokale Deklarationssnode besucht wird, versucht der Algorithmus nun, die variable durch eine freie passive Variable zu ersetzen. Dafür wird die Hashfunktion auf die Variable angewendet, die passive variables map durchsucht. Wird eine passende freie passive Variable gefunden, wird in einer separaten Map ein index hochgezählt, der die Variable als belegt markiert und den index der nächsten freien Variable im vector widerspiegelt. 
      6. In einer replace map wird nun der name der Variable zu ebenjener passiven Variable gemappt, um zukünftige Codereferenzen auf diese Variable zu ersetzen.
      7. Die Deklarationsnode wird nun durch eine assignment node ersetzt. War im ursprünglichen Deklarationsstatement ein Wert zugewiesen, so wird dieser in die assignment node übernommen, wenn nicht wird ein typ neutraler wert zugewiesen.
      8. Gemäß des Visitor Patterns werden nun alle Variablen Referenzen bei Besuch durch die in der replace map gemappten Variablen ersetzt und erhalten gleichzeitig einen neuen Pointer zu ihrer neuen Deklaration, um diese eindeutig zu kennzeichnen, da in diesem Schritt in manchen Fällen bereits name clashes auftreten können.
        ```cksp
          declare i: int
          if(i = NUM_GROUPS)
              declare i: int
              message("If-Variable j: " & i)
          else
              declare j: int
              message("Else-Variable i: " & j)
          end if
        ```
      Im obigen Beispiel wird die Variable j im elseblock durch die passive lokale variable i ersetzt, die im if block deklariert wurde. Die Referenz auf j wird durch einen Pointer auf die Deklaration von i ersetzt. Würden nur Namen geändert werden, würde die eindeutige Zuordnung verloren gehen.
      9. Am Ende eines jeden Scopes werden die entsprechenden Maps geleert, um für den nächsten Scope bereit zu sein.
      10. Währenddessen wurden weak ptr zu allen Deklarationen und Referenzen in globalen Vektoren gespeichert, um später auf sie zugreifen zu können.
      11. Um nun Namenskollisionen zu vermeiden, wird durch den Vektor aller Deklarationen iteriert und alle lokalen Variablen erhalten mittels Gensym einen neuen Namen, um Kollisionen zu vermeiden. Im Anschluss werden mittels Referenzen Vektor alle Referenzen auf diese Variablen ebenfalls umbenannt, indem sie den gleichen Namen annehmen, wie die Variable, auf die sie durch bereits angesprochenen Pointer zeigen.
      12. Der Register Reuse Prozess endet mit der eigentlichen promotion zu globalen Variablen, nachdem alle passiven Variablen wenn möglich wiederverwendet wurden. Dazu werden alle übrig gebliebenen lokalen Deklarationstatements durch assignments ersetzt und ohne Value zuweisung an den Anfang des on init Callbacks verschoben.

      ```
        function register_reuse(ASTNode node) {
          visit(node)
        }

        function visit(NodeBlock node) override {
            
        }

        function visit(NodeDeclaration node) override {

        }



      ```


   - **Parameter Promotion** (Lambda Lifting):  
    Bei der Beschreibung des Register Reuse Algos haben wir Funktionen zunächst außer acht gelassen. Doch was ist mit lokalen Variablen in Funktionen und wie gelangen sie in den on init callback?
     - Lokale Variablen werden in Funktionsparameter umgewandelt.  
     - Beispiel:  
       ```ksp
       // Pre-Transformation
       function foo() {
           declare x: int := 10;
           message(x);
       }
       // Post-Transformation
       declare x_global: int;
       function foo(x_param: int) {
           x_global := x_param;
           message(x_global);
       }
       ```

5. **Optimierungen und Lösungen**  
   - **Array Assignment Lowering**:  
     - Arrays werden durch Iterator-Funktionen initialisiert, um KSP-Restriktionen zu umgehen.  
     ```ksp
     declare local _iter: int;
     array.init.Integer(arr, _iter, 0);
     
     function array.init.Integer(array: int[], iter: int, value: int)
         while (iter < num_elements(array))
             array[iter] := value;
             inc(iter);
         end while
     end function
     ```  
   - **Race Condition Management**:  
     - Thread-unsichere Operationen werden erkannt und gekennzeichnet.  
     - **Ziel**: Konsistente Variablenzustände trotz asynchroner Callbacks.  
   - **Speicheroptimierung**:  
     - Reduktion der Variablenanzahl durch dynamische Wiederverwendung.  

6. **Evaluation der Algorithmen**  
   - **Vorher-Nachher-Vergleich**:  
     - Beispiel für Code vor und nach der Transformation.  
   - **Performance-Vergleich**:  
     - Reduzierung der Variablenanzahl.  
     - Verbesserung des Speicherverbrauchs.  

