# Format générique de capture CAN V2

Le format V2 est indépendant de BMW. Il sépare les trames reçues du manifeste
de session afin qu'une capture puisse être validée, comparée et archivée sans
injecter de métadonnées dans le flux temporel.

La Phase 3 définit le modèle et le schéma. Le transport et l'outil PC qui
matérialiseront automatiquement ces deux fichiers appartiennent à la Phase 4.

## Fichier de trames

Extension recommandée : `.cantrace.v2.csv`. Encodage UTF-8, fin de ligne LF,
sans séparateur de milliers. L'en-tête exact est :

```text
timestamp_us,sequence,channel,bitrate,id,extended,dlc,data,direction,status
```

| Champ | Contrat |
|---|---|
| `timestamp_us` | microsecondes monotones depuis le boot, entier non signé |
| `sequence` | numéro strictement croissant attribué avant la file logicielle |
| `channel` | numéro logique du bus de banc, 0 à 255 |
| `bitrate` | `100000` ou `500000` en Phase 3 |
| `id` | identifiant CAN hexadécimal préfixé par `0x` |
| `extended` | `0` pour 11 bits, `1` pour 29 bits |
| `dlc` | longueur classique reçue, 0 à 8 |
| `data` | exactement `dlc * 2` chiffres hexadécimaux, vide si DLC 0 |
| `direction` | toujours `RX` ; aucune valeur TX n'est définie |
| `status` | masque hexadécimal des drapeaux génériques de réception |

Les timestamps peuvent être égaux ; `sequence` tranche toujours l'ordre. Un
trou de séquence représente une perte dans la file logicielle après réception.
Les pertes antérieures dans le pilote sont documentées par le manifeste.

## Manifeste distinct

Extension recommandée : `.manifest.v2.json`. Le schéma machine est
[`schemas/can-capture-session-v2.schema.json`](../schemas/can-capture-session-v2.schema.json).
Il exige notamment :

- version du format et version du firmware ;
- identifiant de session non personnel ;
- interface et identifiant de configuration ;
- débit et mode constant `listen-only` ;
- total reçu, pertes de la file logicielle, pertes du pilote, overruns FIFO et
  erreurs bus ;
- transitions warning, error-passive, bus-off et resets lorsqu'elles existent.

`vehicle_information` est facultatif. Aucun VIN, nom, position ou autre donnée
personnelle n'est requis.

Pour une future capture K-CAN, `interface` et `configuration_id` doivent
identifier sans ambiguïté la couche physique, par exemple
`KCAN_LSFT_ISO11898_3_RX_ONLY`, ainsi que la révision du banc. Le débit
`100000` ne suffit pas à distinguer K-CAN d'un banc ISO 11898-2 ralenti. Le
fichier de trames reste générique et ne contient aucun nom de signal BMW.

## Drapeaux de statut

| Bit | Signification |
|---:|---|
| 0 | remote frame |
| 1 | DLC non conforme tronqué à huit octets |
| 2 | contrôleur au-dessus du seuil d'avertissement |
| 3 | contrôleur error-passive |
| 4 | état bus-off observé |
| 5 | reset périphérique observé |

Les drapeaux décrivent l'état générique de réception. Ils ne constituent ni un
décodage métier ni une interprétation BMW.
