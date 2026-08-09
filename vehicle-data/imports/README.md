# Import générique de données tabulaires

Le format intermédiaire est volontairement indépendant d'ISTA, TestO, INPA et
Tool32. Il accepte un fichier à en-tête délimité par virgule, point-virgule,
tabulation ou barre verticale, puis un mapping explicite des colonnes.

Champs canoniques requis : `timestamp_us`, `phase`, `signal`, `raw_value`,
`interpreted_value`, `unit`, `confidence` et `source_locator`. Le mapping fixe
aussi la session et la preuve commune à l'export importé.

L'importeur refuse notamment : colonne absente, timestamp non entier ou non
monotone, séquence incohérente, signal/phase inconnu, preuve absente, valeur sans
provenance et unité incohérente. Il n'infère aucun signal BMW et n'accepte aucun
identifiant CAN.
