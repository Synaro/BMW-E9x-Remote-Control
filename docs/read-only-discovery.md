# Protocole de découverte en lecture seule

## Objectif

Identifier progressivement des sources candidates pour les signaux sémantiques
du projet sans transmettre de commande depuis le contrôleur. Une corrélation
trouvée dans une trace reste une hypothèse : elle ne devient `Verified` qu'après
des répétitions, des contrôles négatifs et une validation indépendante.

Ce protocole sépare deux sources possibles :

- **diagnostic K+DCAN** : inventaire des calculateurs et lecture de valeurs
  exposées par les outils BMW ;
- **capture CAN brute** : journal passif produit par une interface et un logiciel
  capables de l'exporter dans un format reconnu par `python-can`.

Le câble K+DCAN ne doit pas être supposé capable de fournir une capture passive
brute. Tant que l'interface de journalisation n'est pas identifiée et qualifiée,
on reste à l'inventaire diagnostic en lecture seule.

Ici, « lecture seule » décrit l'absence de codage ou d'action métier : un outil
diagnostic transmet tout de même des requêtes au véhicule. Cette phase n'est
donc pas une écoute passive du bus.

## Phase A — Inventaire diagnostic

Pour le véhicule de référence, noter sans publier le VIN :

- profil attendu : `bmw-e90-2009-n47d20c-automatic` ;
- modèle, année, moteur et transmission réellement confirmés ;
- calculateurs présents et leurs versions logicielles ;
- noms des valeurs d'état disponibles en lecture seule ;
- outil et version employés ;
- état du contact et du moteur pendant chaque observation ;
- erreurs de communication rencontrées.

Ne lancer aucune fonction de codage, programmation, effacement de défaut,
activation d'actionneur ou exécution de job dont l'effet n'est pas établi.

## Phase B — Paire de captures contrôlée

Cette phase exige une interface de capture brute configurée sans émission.
L'outil fourni accepte uniquement les chemins silencieux déjà documentés et
intégrés ; voir [capture-qualification.md](capture-qualification.md). Son refus
d'un pilote ne signifie pas que le matériel est inutilisable, mais qu'il n'est
pas encore qualifié par ce projet.

Conditions minimales :

1. véhicule immobilisé, boîte automatique en `P`, frein de stationnement appliqué ;
2. moteur arrêté et aucune demande de démarrage distant ;
3. état du contact identique entre les deux captures ;
4. durée de capture identique ;
5. une seule action observable modifiée ;
6. aucune personne ni objet dans une zone mobile ou dangereuse.

Commencer uniquement par des événements de carrosserie à faible risque, par
exemple ouvrir puis refermer une porte ou le coffre moteur arrêté. Le véhicule de
référence ne possédant pas de capteur de capot exploitable, une capture du bus ne
pourra pas faire apparaître cette information. Cette installation peut laisser
le signal désactivé avec `requireHoodClosed = false` ; aucune recherche CAN du
capot n'est alors nécessaire. Ne pas commencer par un changement de rapport, un
relâchement du frein de stationnement ou une commande moteur.

Pour chaque événement :

1. enregistrer une trace de référence stable ;
2. reproduire exactement le même contexte avec un seul événement ;
3. répéter au moins trois paires indépendantes ;
4. réaliser également le changement inverse ;
5. conserver les fichiers dans `captures/private/<session>/`.

Lorsque l'interface fait partie des pilotes acceptés, la trace canonique peut
être créée directement avec `scripts/capture-can-trace.ps1`. Dans les autres cas,
produire un journal avec l'outil du fabricant puis utiliser l'importeur, sans
contourner le refus du mode silencieux.

## Import et comparaison

Convertir chaque journal brut :

```powershell
python .\tools\import_can_trace.py `
  .\captures\private\session-01\baseline.asc `
  .\captures\private\session-01\baseline.cantrace.csv

python .\tools\import_can_trace.py `
  .\captures\private\session-01\door-open.asc `
  .\captures\private\session-01\door-open.cantrace.csv
```

Comparer ensuite les distributions d'octets :

```powershell
.\scripts\analyze-trace-change.ps1 `
  .\captures\private\session-01\baseline.cantrace.csv `
  .\captures\private\session-01\door-open.cantrace.csv
```

Le score combine la distance entre distributions et la stabilité de la valeur
la plus fréquente dans chaque trace. Le masque `xor` indique uniquement quels
bits diffèrent entre les deux modes observés. Les identifiants présents dans une
seule trace sont signalés séparément, car une absence peut aussi provenir d'un
problème de capture.

Tester l'outil sans matériel :

```powershell
.\scripts\analyze-trace-change.ps1 `
  .\scenarios\synthetic_idle.cantrace.csv `
  .\scenarios\synthetic_hood_open.cantrace.csv
```

Le résultat synthétique attendu place l'identifiant `0x1FFFFF01`, octet 0, en
tête avec le masque `0x01`. Cet identifiant appartient exclusivement au
simulateur et n'est pas une donnée BMW. Ce scénario vérifie l'analyseur ; il ne
suggère pas qu'un signal de capot existe sur le véhicule de référence.

## Critères avant écriture d'un décodeur

Un candidat ne peut entrer dans un décodeur BMW lecture seule que si :

- le changement est reproduit dans plusieurs paires ;
- les captures inverses confirment le sens du signal ;
- des contrôles sans événement ne produisent pas le même résultat ;
- la cadence et la fraîcheur sont mesurées ;
- les valeurs invalides et absentes sont définies ;
- la variante véhicule concernée est explicitement enregistrée ;
- les tests de rejeu couvrent perte, retard et corruption.

Même après cela, le profil reste `Candidate` jusqu'à la qualification lecture
seule complète. Les captures ne doivent jamais être ajoutées au dépôt sans
anonymisation et revue explicite.
