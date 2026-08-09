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

`tools/import_engine_speed_log.py` traite séparément les CSV de régime moteur.
Son mapping configure les colonnes, l'unité de temps et des seuils explicitement
`candidate_only`. La classification exige plusieurs échantillons consécutifs et
une hystérésis ; elle reste observationnelle et n'alimente aucun contrôleur.

Le mapping `current-test-vehicle-testo-engine-speed.mapping.json` est incomplet
par conception tant que le CSV brut n'est pas disponible : ses noms de colonnes
doivent être remplacés après inspection du fichier réel.
