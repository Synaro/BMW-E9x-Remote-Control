# Contribuer

## Règles d'architecture

- Ne pas inclure de dépendance Arduino, ESP32, CAN ou GPIO dans `domain` ou
  `application`.
- Ne pas introduire d'allocation dynamique dans le chemin de décision.
- Toute condition inconnue doit échouer de manière sûre.
- Une action physique doit passer par un port d'infrastructure.
- Toute nouvelle transition doit être couverte par au moins un scénario nominal
  et un scénario d'échec.
- Ne pas ajouter de fonctionnalité de contournement d'antidémarrage ou d'antivol.
- Ne jamais présenter les identifiants du protocole synthétique comme des
  identifiants BMW ni les transmettre sur un bus réel.
- Ne pas committer de capture privée contenant un VIN, une position, une clé ou
  une autre donnée personnelle.

## Vérification

Sous Windows :

```powershell
./scripts/test.ps1
./scripts/simulate.ps1
pio run -e native
```

Les changements destinés à une carte réelle doivent également fournir les
résultats de compilation de la cible et le protocole d'essai associé.

## Style

- C++17 ;
- types de taille explicite dans le cœur embarqué ;
- fonctions `noexcept` sur le chemin de décision ;
- tableaux de capacité fixe ;
- noms anglais dans le code, documentation possible en français ;
- avertissements du compilateur activés et traités avant fusion.
