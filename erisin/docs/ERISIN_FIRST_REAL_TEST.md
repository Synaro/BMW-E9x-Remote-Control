# Premier test réel Erisin — bridge EDIABAS en lecture seule

Ce test qualifie séparément l'installation des deux APK, la détection FTDI,
la permission USB, la configuration EDIABAS, la réponse `IDENT` d'un SGBD et
l'inventaire de ses jobs. Il ne valide aucune commande d'éclairage.

## Préconditions

- Erisin ES3360I sous Android 10/API 29 avec ADB disponible.
- Câble FTDI K+DCAN branché entre l'USB de l'Erisin et la prise diagnostic de
  la BMW.
- Véhicule immobilisé, frein de stationnement serré, batterie correctement
  alimentée et contact dans l'état requis pour le diagnostic.
- Un fichier `.PRG` provenant légalement de l'installation BMW Standard Tools
  de l'utilisateur, copié dans un emplacement lisible par le sélecteur de
  fichiers Android. Ne jamais ajouter ce fichier au dépôt Git.

## Installation

Depuis le répertoire contenant les artefacts CI :

```powershell
adb install -r .\BMW-E9x-EdiabasBridge-debug.apk
adb install -r .\BMW-E9x-Control-debug.apk
```

Le bridge doit être installé en premier. Les deux commandes doivent se terminer
par `Success`.

## Test couche par couche

1. Brancher le K+DCAN sur un port USB hôte de l'Erisin, puis lancer **BMW E9x
   Control**.
2. Ouvrir **DIAGNOSTICS** et toucher **START BRIDGE**. Attendre quelques
   secondes : `Bridge` doit passer à `CONNECTED`, et les versions du bridge et
   d'EdiabasLib doivent être renseignées.
3. Vérifier `FTDI`. Le champ doit afficher au minimum le VID `0403`, le PID et,
   si le périphérique l'expose, son numéro de série.
4. Toucher **CONNECT** une première fois. C'est cette opération qui demande la
   permission USB Android au nom du bridge .NET. Accepter la boîte de dialogue.
   Le premier appel peut signaler que la permission vient d'être demandée ;
   c'est attendu.
5. Revenir à **DIAGNOSTICS** et vérifier `USB permission: GRANTED`. Si le champ
   n'est pas encore actualisé, toucher **CONNECT** une seconde fois. Vérifier
   ensuite `EDIABAS: CONNECTED (SESSION; VEHICLE RESPONSE NOT YET IMPLIED)` et
   contrôler que `ECU path` pointe vers le stockage privé du bridge.
6. Toucher **IMPORT PRG**, sélectionner le `.PRG` utilisateur, puis vérifier
   que `Imported PRG` a augmenté. L'APK Java conserve sa copie privée et la
   transfère au bridge par loopback avec son SHA-256 ; aucun PRG ne doit passer
   par Git.
7. Toucher de nouveau **CONNECT** si la session a été interrompue, puis toucher
   **DISCOVER FRM** une seule fois.
8. **DISCOVER FRM** effectue, pour chaque PRG importé, les opérations de lecture
   suivantes : transfert vérifié, chargement du SGBD, recherche du job
   `IDENT`, exécution de `IDENT`, lecture de `_JOBS`, `_JOBCOMMENTS`,
   `_ARGUMENTS` et `_RESULTS`, puis création d'un dump JSON. Il n'exécute aucun
   job de commande lampe.
9. Vérifier les champs `SGBD`, `FRM`, `Last job`, `Job execution`, `IPC`,
   `Round trip` et `Last error`. Un SGBD n'est considéré comme répondant que si
   `IDENT` a réellement réussi ; un bridge compilé ou une session configurée ne
   suffit pas.

## Afficher et filtrer la liste des jobs

Le debug APK écrit le dump sous
`files/ediabas/jobs/frm_jobs_<sgbd>.json`. Lister d'abord le nom exact :

```powershell
adb shell run-as com.synaro.bmwe9xcontrol.debug ls files/ediabas/jobs
```

Remplacer `<fichier.json>` par le nom retourné, puis exporter le dump :

```powershell
adb exec-out run-as com.synaro.bmwe9xcontrol.debug cat files/ediabas/jobs/<fichier.json> > frm_jobs.json
Select-String -Path .\frm_jobs.json -Pattern 'STEUERN|LAMPE|LAMPEN|PWM|AUSGANG|LICHT' -CaseSensitive:$false
```

Conserver le JSON et le journal de test comme preuves. Les correspondances sont
des **candidats documentés par le SGBD**, pas des capacités déjà validées sur le
véhicule.

## Critère d'arrêt

S'arrêter après `IDENT` et l'inventaire filtré. Ne pas armer une capacité, ne
pas sélectionner le sink FRM réel et ne lancer ni **LIGHTS**, ni **GHOST**, ni
**SHOWS**, ni **MUSIC** pendant cette première session. En cas d'erreur,
relever exactement `Last error`, les trois durées et l'étape qui a échoué avant
toute nouvelle tentative.
