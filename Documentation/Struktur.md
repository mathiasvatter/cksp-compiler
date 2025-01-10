## Struktur

### 1. Einleitung
- Einführung in das Thema, 
    - musikindustrie, filmmusik -> virtuelle instrumente
    - was ist Kontakt -> Industriestandard
    - was ist KSP, warum ist es wichtig
    - realtime audio processing -> speicherplatz und performance
    - wird in ram geladen?
    - pascal syntax
    - 2004?
- Motivation für funktionale Programmierkonzepte in KSP. 
    - Warum nicht gleich in der Sprache? -> underfunded und closed-source
    - Effizienz und Flexibilität?
- Zielgruppe?
  - Kontakt-Entwickler, meistens Musiker und Sounddesigner mit wenig Programmiererfahrung.
- Zielsetzung und Problemstellung: Herausforderungen bei der Umsetzung.

### 2. Hintergrund
- **KSP Tutorial der Syntax und ihre Limitierungen**
    - Beispiel Code
        - on init, on note, on release callback-driven
        - Variablen, Arrays, Strukturen
        - funktionen
        - strong typing
- 
<!-- ### Einführung in Callbacks in KSP

1. **Grundlage des Kontrollflusses in KSP**:
   - KSP arbeitet auf Basis eines **Event-Driven Models**, wobei Callbacks zentrale Steuerungselemente sind.
   - Callbacks wie `on init` und `on note` definieren, wie und wann bestimmte Code-Blöcke ausgeführt werden:
     - `on init`: Wird einmalig beim Laden des Scripts ausgeführt.
     - `on note`, `on release`: Werden durch User-Input oder MIDI-Events getriggert.
   - Die Reihenfolge der Callback-Ausführung wird primär durch Benutzeraktionen und nicht durch den Entwickler kontrolliert.

2. **Synchronität und Variablenlebensdauer**:
   - Innerhalb eines Callbacks werden alle Anweisungen sequenziell ausgeführt, bevor der nächste Callback aktiviert wird.
   - Variablen, die in mehreren Callbacks referenziert werden, behalten ihren Zustand, was ihren "globalen" Charakter unterstreicht.

3. **Besonderheiten und Herausforderungen**:
   - **Asynchrone Operationen**: Funktionen wie `wait` erlauben Unterbrechungen, während andere Callbacks ausgeführt werden können.
   - **Race Conditions**: Diese treten auf, wenn mehrere Callbacks dieselben Variablen verändern, bevor der ursprüngliche Prozess abgeschlossen ist.
   - **Wiedereintrittsprobleme**: Wenn ein Callback erneut aufgerufen wird, während er noch läuft, können undefinierte Zustände entstehen. -->




- Sublime KSP Syntax und Limitierungen
    - Beispiel Code
- CKSP Compiler pre-thesis?

<!-- ### 3. Implementierung von Funktionen mit Parametern und return statements
- **Functions in KSP**:
    - Funktionen ohne parameter mit `call` keyword
- **Vorgehen**:
    - Typisierte Funktionen mit default Werten
    - Inlining von expression-only functions
    - Function Call Hoisting, Return Parameter Rewriting
    - Function Inlining
- **Herausforderungen**:
    -  -->

### 4. Von Lexical to Global Scope mittels Variable Reuse und Parameter Promotion
- **Control Flow Management in KSP**:
    - Analyse des Callbacks-basierten Kontrollflusses.
    - Herausforderungen:
        - Race Conditions bei asynchronen Callbacks.
        - Race Conditions bei Wiedereintritt
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

### 5. Implementierung Rekursiver Datenstrukturen in CKSP
- **Lowering rekursiver Strukturen**:
    - **Vorgehen**:
    - Einführung neuer AST Nodes und des Typs `Object`.
    - Detaillierte Transformation von Strukturen in Arrays und Pointern.
        - **Desugaring und Syntax-Transformationen**:
            - Vorbereitende Schritte wie Namensräume und Operator Overloading.
        - **Type Inference und Typüberprüfung**:
            - Automatische Typinferenz für rekursive Strukturen und Access Chains.
        - **Pre-Lowering und Lowering**:
            - Anpassungen für Variablen und Member, um sie als Arrays zu repräsentieren.
            - Transformationen zur Darstellung rekursiver Strukturen als eindimensionale Speicherarrays.
    - **Herausforderungen**:
        - Begrenzungen durch KSP-Speicherhandling.
        - Komplexität bei der Referenzzählung.
- **Reference Counting und Speicherverwaltung**:
    - **Vorgehen**:
        - Beschreibung der Methoden `__incr__`, `__decr__`, und `__del__` für Speicherverwaltung.
        - Implementierung für verschiedene Rekursionstypen:
            - **Lineare direkte Rekursion**: Einfachste Methode, kein Stack erforderlich.
            - **Nicht-lineare direkte Rekursion**: Mehrere rekursive Mitglieder, Stack erforderlich.
            - **Nicht-lineare indirekte Rekursion**: Indirekte Beziehungen zwischen Strukturen, komplexer Stack-Einsatz.
    - **Herausforderungen**:
        - Unterschiedliche Anforderungen an Speicherverwaltung je nach Rekursionstyp.
        - Iterative Implementierung aufgrund der fehlenden Rekursionsunterstützung in KSP.

### 5. **Evaluation und Analyse**
   - **Effizienz des Register Reuse Algorithmus**:
      - Vergleich mit Sublime KSP Compiler
   - **Effizienz der Speicherverwaltung**:
      - Vergleich der Reference Counting-Methoden und deren Auswirkungen auf Speicherverbrauch und Performance.

### 6. **Fazit und Ausblick**
   - Zusammenfassung der wichtigsten Ergebnisse und Herausforderungen.
   - **Kompilierungszeiten und Optimierungspotenzial**:
      - Analyse der Kompilierungszeiten für komplexe Strukturen und Funktionen.
   - **Implementierte Funktionale Konzepte in KSP**:
      - Evaluation der Praxistauglichkeit für reale Musikproduktionsszenarien in KSP.
   - Ausblick auf zukünftige Verbesserungen und Optimierungsmöglichkeiten im Compilerbau für domänenspezifische Sprachen.

