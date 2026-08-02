# Adaptateur CAN de commande de verrouillage

## Rôle

`CanLockCommandAdapter` est la frontière de lecture seule entre une trame CAN
qualifiée et `LockCommandEvidence`. Il ne possède aucune fonction d'émission et
ne connaît aucun identifiant BMW par défaut. Sa configuration initiale est
désactivée et non vérifiée.

`CanLockCommandPipeline` relie cet adaptateur à `LockCommandGate`. Une anomalie
structurelle réinitialise les deux composants afin qu'un appui reconnu avant une
trame douteuse ne puisse pas être complété après celle-ci.

## Liaison à qualifier

Une configuration activée doit fournir :

- l'identifiant, le type standard ou étendu et le DLC exacts ;
- un prédicat de bits représentant la commande de verrouillage ;
- un prédicat indépendant confirmant que le véhicule est sécurisé ;
- un compteur roulant indépendant d'au moins deux bits ;
- un niveau de confiance explicite.

Les trois champs ne peuvent pas partager un même bit. Les bits du compteur sont
extraits d'un octet et compactés, ce qui accepte un masque non contigu. Cette
forme volontairement limitée évite d'ajouter une interprétation Motorola/Intel
avant d'avoir observé le format réel.

Cette première implémentation exige aussi que commande, état sécurisé et
compteur appartiennent à la même trame. Si les observations BMW montrent des
sources séparées, un agrégateur de preuves fraîches devra être conçu et testé ;
la contrainte ne devra pas être contournée en inventant une valeur constante.

Le vecteur `0x321` utilisé par les tests et le simulateur est fictif. Il ne doit
jamais être copié dans un profil BMW.

## Comportement temporel

La première trame reconnue amorce le décodeur mais ne produit jamais de preuve.
Il faut ensuite observer un front montant du prédicat de commande : maintenir
le même état sur plusieurs trames ne compte que pour un seul appui.
Une liaison dont la commande ne revient pas à un état inactif entre deux appuis
n'est donc pas compatible tant qu'une sémantique d'événement distincte n'a pas
été qualifiée et implémentée.

Chaque trame reconnue doit faire avancer le compteur dans la moitié avant de sa
plage. Le rebouclage est accepté. Un compteur identique, revenu en arrière ou
ambigu, un DLC inattendu ou un recul d'horodatage provoque un rejet et efface la
séquence partielle. Les trames ayant un autre identifiant sont simplement
ignorées et ne perturbent pas un geste valide.

Une preuve produite contient le compteur étendu, l'horodatage de la trame, le
niveau de confiance configuré et le résultat du prédicat « véhicule sécurisé ».
La garde applicative effectue encore ses propres contrôles de confiance, de
fraîcheur et d'ordre avant de compter l'appui.

## Qualification BMW requise

Avant de définir une liaison réelle, les captures privées doivent démontrer :

1. que la commande réagit une fois par appui et revient à son état inactif ;
2. que l'état sécurisé est indépendant de la commande et correspond au véhicule ;
3. que le compteur évolue sur chaque trame reconnue, avec sa largeur et son sens ;
4. que le comportement se répète lors du verrouillage, du déverrouillage et des
   contrôles sans action ;
5. que la cadence, les pertes, le rebouclage et les valeurs invalides sont connus ;
6. que la provenance CAS/JBE retenue est confirmée pour la variante visée.

K+DCAN peut aider à observer les états diagnostic CAS/JBE, mais ne remplit pas à
lui seul ces critères de capture CAN brute. Tant que les preuves ne sont pas
collectées et revues, la liaison reste absente, le profil reste `Discovery` et
aucune commande réelle ne peut être marquée `Verified`.
