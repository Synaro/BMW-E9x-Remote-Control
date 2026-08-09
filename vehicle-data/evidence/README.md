# Provenance des preuves

L'index JSON relie un identifiant stable à un fichier de preuve. Les chemins
sont relatifs à ce dossier, sans chemin absolu ni `..`. Les captures contenant
un VIN, une immatriculation ou d'autres données personnelles restent dans
`private/`, ignoré par Git, jusqu'à création d'une copie anonymisée explicite.

Une entrée peut décrire :

- `ISTA_CAPTURE` ;
- `TESTO_EXPORT` ;
- `INPA_OUTPUT` ;
- `TOOL32_OUTPUT` ;
- `FUTURE_CAN_CAPTURE` ;
- `MANUAL_NOTE`.

`sha256` est facultatif pendant la saisie, mais recommandé avant de faire passer
une preuve au niveau `VALIDATED`. L'empreinte doit correspondre aux octets du
fichier référencé.
