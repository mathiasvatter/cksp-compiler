#!/bin/bash

# Überprüfen, ob ein Dateiname als Argument übergeben wurde
if [ "$#" -ne 1 ]; then
  echo "Verwendung: $0 <Markdown-Datei>"
  exit 1
fi

# Der Name der Markdown-Datei
md_file="$1"

# Konvertierung zu PDF mit Pandoc, inklusive Inhaltsverzeichnis, Anpassungen für listings, Syntax-Highlighting, und erhöhten Abständen vor Überschriften
pandoc "$md_file" \
  --pdf-engine=xelatex \
  --toc \
  -V toc-title:"Contents" \
  -V geometry:margin=2cm \
  -V mainfont="Helvetica Neue" \
  --variable urlcolor=blue \
  --highlight-style=tango \
  --listings \
  -H <(echo '
\usepackage[margin=2cm]{geometry}
\usepackage{listings}
\usepackage{xcolor}
% \usepackage{inconsolata}
\usepackage{fontspec}
\usepackage{titlesec} % Für Abstands-Anpassungen bei Überschriften
\usepackage{enumitem} % Für Anpassungen bei Aufzählungen
\setitemize{left=0pt, noitemsep,topsep=0pt,parsep=0pt,partopsep=0pt} % Reduziert den Abstand zwischen Aufzählungspunkten
\setmainfont{Helvetica Neue}
\setmonofont{Fira Code} % Setzt die Schriftart auf Fira Code
% Stellen Sie sicher, dass das Inhaltsverzeichnis auf einer neuen Seite beginnt
\let\oldtoc\tableofcontents
\renewcommand{\tableofcontents}{\oldtoc\clearpage}

% Inline Code mit grauem Hintergrund
\newcommand{\inlinecode}[1]{\colorbox{gray!30}{\texttt{#1}}}
% Erhöhte Abstände vor Überschriften
\titleformat{\section}
  {\normalfont\LARGE\bfseries}{\thesection}{1em}{}
  [\vspace{0.2em}\hrule\vspace{1em}] % Abstand nach der Überschrift
\titlespacing*{\section}{0pt}{4em}{0em} % Erhöhter Abstand vor der Überschrift

\titleformat{\subsection}
  {\normalfont\Large\bfseries}{\thesubsection}{0.5em}{}
  [\vspace{0.1em}\hrule\vspace{0.6em}] % Abstand nach der Unterüberschrift
\titlespacing*{\subsection}{0pt}{3em}{0em} % Erhöhter Abstand vor der Unterüberschrift

\titleformat{\subsubsection}
  {\normalfont\large\bfseries}{\thesubsubsection}{0.5em}{}
  [\vspace{0.5em}] % Abstand nach der Unter-Unterüberschrift
\titlespacing*{\subsubsection}{0pt}{2em}{0em} % Erhöhter Abstand vor der Unter-Unterüberschrift

\titleformat{\subsubsubsection}
  {\normalfont\large\bfseries}{\thesubsubsubsection}{0.5em}{}
  [\vspace{0.5em}] % Abstand nach der Unter-Unterüberschrift
\titlespacing*{\thesubsubsubsection}{0pt}{1em}{0em} % Erhöhter Abstand vor der Unter-Unterüberschrift

\usepackage{titling} % Für erweiterte Titelgestaltung
\usepackage{array} % Für Tabellenlayout

% Titel formatieren, linksbündig und größer
\pretitle{\vspace*{0em} % Abstand vor dem Titel
          \normalfont\huge\bfseries % Schriftart und -größe größer als bei Subsection
          \begin{flushleft}} 
\posttitle{\end{flushleft}\par\nobreak\vspace{0em} % Abstand nach dem Titel
           \hrule\vspace{0.1em}} % Horizontale Linie unter dem Titel und weiterer Abstand
\preauthor{\begin{flushleft}\large} % Linksbündiger Autor
\postauthor{\end{flushleft}} % Abstand nach dem Autor
\predate{\begin{flushright}\large} % Linksbündiges Datum
\postdate{\end{flushright}\vspace{1em}} % Abstand nach dem Datum


% Importiere die CKSP Syntax-Highlighting Datei
\input{/Users/Mathias/Scripting/ksp-compiler/Documentation/_cksp_syntax_latex/cksp_syntax.ext}
\definecolor{code_bg}{RGB}{254,254,254}

\lstset{
  language=CKSP,
  style=basicCKSP,
  % basicstyle=\sffamily,
  basicstyle=\ttfamily\footnotesize,
  % numbers=left,
  % numberstyle=\tiny\color{gray},
  stepnumber=1,
  numbersep=10pt,
  showstringspaces=false,
  tabsize=3,
  breaklines=true,
  breakatwhitespace=false,
  captionpos=b,
  backgroundcolor=\color{code_bg},
  breaklines=true,
  frame=tb, %single,                               % Einzelner Rahmen um Codeblöcke
  framesep=3mm,                               % Abstand des Rahmens zum Code
  framerule=0.5pt,                              % Dicke der Rahmenlinie
%   xleftmargin=2mm,                            % Linker Innenabstand
%   xrightmargin=2mm,                           % Rechter Innenabstand
  aboveskip=1em,                              % Abstand über dem Codeblock
  belowskip=1em                               % Abstand unter dem Codeblock
}
') \
  -o "${md_file%.md}.pdf"

echo "Konvertierung abgeschlossen: ${md_file} -> ${md_file%.md}.pdf"
