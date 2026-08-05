# Configuration utilisateur

## Principe

Les préférences utilisateur sont représentées par `UserSettings`, indépendamment
du matériel et du format de stockage. Elles sont validées avant de construire
`ControllerConfig` et `LockSequenceConfig`. Une valeur invalide désactive le
démarrage distant ; elle n'est jamais tronquée ou remplacée silencieusement.

Le simulateur et le configurateur Windows fournissent un adaptateur de fichier
`key=value`. Le prototype ESP32-S3 utilise le même modèle dans sa mémoire flash
et le configurateur le lit ou l'écrit par USB.

## Réglages disponibles

| Clé | Valeurs | Défaut | Effet |
|---|---|---:|---|
| `remote_start_enabled` | `true`, `false` | `true` | Interrupteur général du démarrage distant |
| `hood_monitoring` | `required`, `disabled` | `required` | Exige ou ignore entièrement le signal de capot |
| `remote_run_minutes` | 1 à 60 | 15 | Durée maximale avant arrêt automatique |
| `driver_entry_mode` | `require_takeover`, `stop_immediately` | `require_takeover` | Reprise temporisée ou arrêt dès l'ouverture d'une porte |
| `takeover_timeout_seconds` | 10 à 300 | 60 | Délai de confirmation de reprise |
| `lock_press_count` | 2 à 5 | 3 | Nombre d'appuis sur verrouillage |
| `lock_minimum_gap_ms` | 50 à 5 000 | 80 | Filtrage des rebonds |
| `lock_maximum_gap_ms` | 50 à 5 000 | 1 500 | Intervalle maximal entre deux appuis |
| `lock_sequence_window_ms` | 500 à 15 000 | 3 000 | Durée totale maximale de la séquence |
| `feature.<code>` | `true`, `false` | `false` | Demande indépendante d'une fonctionnalité du catalogue |

Les temporisations de verrouillage doivent aussi être cohérentes entre elles.
Par exemple, l'intervalle minimal ne peut pas dépasser l'intervalle maximal et
la fenêtre totale doit pouvoir contenir le nombre d'appuis choisi.

`hood_monitoring=disabled` correspond au véhicule de référence dépourvu de
capteur. Aucune valeur fictive « capot fermé » n'est créée : le signal est
réellement retiré de la décision.

## Fonctionnalités modulaires

Le fichier accepte les 43 identifiants stables du catalogue sous la forme :

```ini
feature.cold_engine_guard=true
feature.alarm_push_notification=false
feature.smartphone_voice_assistant=false
```

Chaque option absente ou à `false` reste désactivée. Une clé inconnue, dupliquée
ou une valeur autre que `true`/`false` fait rejeter le fichier complet.

Une préférence à `true` exprime une demande ; elle ne contourne ni l'absence
d'implémentation, ni une capacité matérielle manquante, ni la qualification BMW,
ni les barrières imposées aux commandes critiques. Les détails et la liste des
niveaux de livraison sont dans
[feature-framework.md](feature-framework.md).

Les fonctions génériques liées au téléphone acceptent une future application
iOS ou Android. Le format de configuration est identique sur les deux
plateformes ; l'application compagnon n'est pas encore livrée.

## Créer sa configuration avec l'assistant

Lancer le configurateur depuis la racine du dépôt :

```powershell
.\scripts\configure.ps1
```

Il charge `config/user-settings.conf` s'il existe, sinon il part des valeurs par
défaut. Entrée conserve la valeur affichée. Le fichier final est relu et validé
avant de remplacer l'ancienne version ; une saisie interrompue ou invalide ne
détruit pas la configuration précédente.

À la fin, l'assistant propose facultativement de parcourir le catalogue des 43
fonctionnalités. Répondre `non` conserve le masque existant ; répondre `oui`
permet d'activer ou désactiver chaque entrée séparément.

Afficher ou contrôler le fichier sans le modifier :

```powershell
.\scripts\configure.ps1 -Show
.\scripts\configure.ps1 -Check
```

Le fonctionnement complet et la CLI sont décrits dans
[configurator.md](configurator.md).

## Édition manuelle facultative

Copier le modèle sans modifier le fichier suivi par Git :

```powershell
Copy-Item `
  .\config\user-settings.example.conf `
  .\config\user-settings.conf
```

`config/user-settings.conf` est ignoré par Git. Il peut ensuite être édité avec
le Bloc-notes ou VS Code.

Afficher et valider les valeurs sans lancer de scénario :

```powershell
.\scripts\build-simulator.ps1
.\build\bmw_remote_simulator.exe `
  --show-config .\config\user-settings.conf
```

Exécuter le parcours configuré :

```powershell
.\scripts\simulate.ps1 `
  -ConfigPath .\config\user-settings.conf
```

Le menu interactif du simulateur propose la même fonction au choix `6`.

Le configurateur ne transmet aucune commande au véhicule. Il peut transmettre
uniquement `UserSettings` au prototype par USB, puis relire la valeur persistée.
Le fichier PC reste utilisable par le simulateur.

Le format de trame indépendant du transport et les conditions d'autorisation
sont définis dans [settings-protocol.md](settings-protocol.md). Un réglage
reçu est enregistré uniquement au repos et prend effet au prochain démarrage du
boîtier ; il ne transforme jamais une session distante en cours.

## Réglages volontairement non désactivables

La configurabilité ne permet pas de supprimer les invariants structurels :

- durée maximale du démarreur ;
- arrêt fail-safe sur panne d'un adaptateur ;
- état de transmission compatible avec le profil ;
- confirmation du régime moteur ;
- cohérence et fraîcheur des signaux utilisés ;
- bornes maximales des temporisations utilisateur ;
- sécurisation des sorties lors d'un refus ou d'un défaut.

Ces éléments relèvent de la sûreté du produit, pas d'une préférence. Les seuils
propres à une variante automobile pourront évoluer dans un profil qualifié, mais
ne seront pas librement modifiables par l'utilisateur final.

## Persistance dans le prototype

`UserSettingsStore` définit les opérations de chargement et d'enregistrement.
Le journal portable fourni utilise deux générations, une version de schéma, un
CRC-32 et un retour aux valeurs sûres si la mémoire est corrompue. Il reste à
raccorder son port `SettingsByteStorage` à une mémoire automobile qualifiée pour
la carte définitive ; le prototype utilise déjà une EEPROM émulée dans la flash
de l'ESP32-S3.
Le contrôleur ne doit être construit qu'après validation réussie des valeurs
chargées.

Une modification ne sera appliquée que moteur arrêté et contrôleur en `Idle`, ou
au redémarrage suivant du boîtier. Elle ne raccourcit, ne prolonge et ne transforme
jamais une session distante déjà active.

Le format et la stratégie de récupération sont détaillés dans
[settings-persistence.md](settings-persistence.md).
