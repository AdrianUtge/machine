# 📡 Améliorations du Moniteur Série React

## 🎯 Résumé des Changements

Le **Moniteur Série** de React a été amélioré pour offrir une expérience complète de **lecture temps réel** et **envoi de commandes manuelles**.

---

## ✨ Nouvelles Fonctionnalités

### 1️⃣ **Input de Commande Manuelle**
```
┌─────────────────────────────────────────────────┐
│ Enter manual command (e.g., HOME, START...)  [Send] │
└─────────────────────────────────────────────────┘
```

- Taper n'importe quelle commande manuelle
- Bouton "Send" pour envoyer la commande
- Désactivé pendant l'envoi (état `isSubmitting`)
- Exemple : `HOME`, `START`, `SET_FREQ:1.5`, `GET_STATUS`

### 2️⃣ **Affichage Étendu des Logs**
- **Avant** : hauteur 192px (h-48) → ~6-8 lignes visibles
- **Après** : hauteur 384px (h-96) → ~12-15 lignes visibles
- Affichage complet de tous les logs récents
- Auto-scroll vers le dernier message

### 3️⃣ **Bouton Rafraîchir (Refresh)**
```
[Filter▼] [↓Auto] [🔄Refresh] [🗑️Clear]
```

- Récupère les 100 derniers logs depuis l'API
- Désactivé pendant le chargement
- Utile pour voir les logs en arrière-plan

### 4️⃣ **Filtrage des Logs**
Déjà existant, complété par l'input manuel :
- All (tous)
- Commands (commandes envoyées)
- Responses (réponses Arduino)
- States (changements d'état)
- Done (commandes complétées)
- Errors (erreurs)

---

## 📝 Format des Messages

### Envoyés (Commands)
```
> HOME
> SET_FREQ:1.5
> GET_STATUS
```

### Reçus (Responses)
```
< STATUS_ACK:OK
< STATE:IDLE
< FREQ:1.5
```

### États et Erreurs
```
[state] Machine changed state
[error] Failed to execute command
```

---

## 🔧 Fichiers Modifiés

### 1. `SerialMonitor.tsx` - Composant Principal
**Changements :**
- ✅ Ajout input + bouton "Send"
- ✅ Augmentation hauteur logs (h-48 → h-96)
- ✅ Ajout bouton refresh
- ✅ Props optionnels : `onSendCommand`, `onRefreshLogs`
- ✅ Gestion d'état : `manualCommand`, `isSubmitting`

**Nouvelles Props :**
```typescript
interface SerialMonitorProps {
  logs: SerialLog[];
  onClear: () => void;
  onSendCommand?: (command: string) => Promise<any>;    // ✨ NOUVEAU
  onRefreshLogs?: () => Promise<void>;                   // ✨ NOUVEAU
  isLoading?: boolean;                                   // ✨ NOUVEAU
}
```

### 2. `App.tsx` - Point d'Entrée
**Changements :**
- ✅ Import `sendManualCommand` et `refreshLogs` du hook
- ✅ Passage des props au composant `SerialMonitor`

```typescript
<SerialMonitor
  logs={serialLogs}
  onClear={() => setSerialLogs([])}
  onSendCommand={sendManualCommand}     // ✨ NOUVEAU
  onRefreshLogs={refreshLogs}           // ✨ NOUVEAU
  isLoading={isLoading}
/>
```

### 3. `useMachineController.ts` - Hook (Inchangé)
Utilise les fonctions existantes :
- `sendManualCommand()` - Envoie une commande manuelle
- `refreshLogs()` - Récupère les logs depuis l'API

---

## 💻 Utilisation

### Via l'Interface Graphique

1. **Envoyer une commande manuelle :**
   ```
   1. Taper dans l'input : "SET_FREQ:2.0"
   2. Cliquer "Send" ou appuyer sur Entrée
   3. La réponse s'affiche immédiatement dans les logs
   ```

2. **Rafraîchir les logs :**
   ```
   1. Cliquer sur le bouton 🔄 Refresh
   2. Les logs sont mis à jour depuis l'API
   ```

3. **Filtrer les logs :**
   ```
   1. Sélectionner un type : "Commands", "Responses", etc.
   2. Seuls les logs de ce type s'affichent
   ```

4. **Auto-scroll :**
   ```
   1. Cliquer sur ↓ Auto
   2. Le moniteur suit automatiquement les derniers logs
   ```

### Exemples de Commandes Manuelles

```bash
HOME                    # Envoyer la machine en position initiale
START                   # Démarrer le mouvement
STOP                    # Arrêter le mouvement
HARD_RESET              # Réinitialiser la machine
GET_STATUS              # Demander l'état complet
SET_FREQ:1.5            # Régler fréquence à 1.5 Hz
SET_SPEED:75            # Régler vitesse à 75%
DEBUG:1                 # Activer le mode debug
?HELP                   # Afficher l'aide Arduino
```

---

## 🎨 Interface Visuelle

### Avant ❌
```
╔═════════════════════════════════════╗
║ Serial Monitor   [▼][↓][🗑️]        ║
╠═════════════════════════════════════╣
║ < STATE:IDLE                        ║
║ < FREQ:1.5                          ║
║ (seulement 6-8 lignes visibles)     ║
╚═════════════════════════════════════╝
```

### Après ✅
```
╔═════════════════════════════════════╗
║ Serial Monitor  [▼][↓][🔄][🗑️]     ║
╠═════════════════════════════════════╣
║ < STATE:IDLE                        ║
║ < FREQ:1.5                          ║
║ > SET_SPEED:75                      ║
║ < STATUS_ACK:OK                     ║
║ < SPEED:75                          ║
║ (12-15 lignes visibles)             ║
╠═════════════════════════════════════╣
║ Enter manual command... [Send]      ║
╚═════════════════════════════════════╝
```

---

## 🚀 Architecture

```
App.tsx
  ├── useMachineController()
  │   ├── sendManualCommand(command)  → API /command/manual
  │   └── refreshLogs()               → API /logs?limit=100
  │
  └── SerialMonitor (amélioré)
      ├── Input + Send (envoi manuel)
      ├── Logs (affichage 12-15 lignes)
      ├── Filtres (All/Commands/Responses/etc.)
      └── Actions (Refresh, Clear, Auto-scroll)
```

---

## ✅ Checklist d'Utilisation

- [x] Envoyer des commandes manuelles via input
- [x] Voir les réponses en temps réel
- [x] Filtrer les types de logs
- [x] Rafraîchir les logs depuis l'API
- [x] Auto-scroll vers les derniers messages
- [x] Nettoyer les logs
- [x] Affichage de plus de lignes (h-96)

---

## 🔍 Dépannage

### Les logs ne s'affichent pas ?
- Vérifier que l'API FastAPI est en cours d'exécution
- Vérifier les logs serveur (terminal)
- Cliquer sur "Refresh" pour recharger

### La commande ne s'envoie pas ?
- Vérifier que l'Arduino est connecté
- Vérifier que la commande est correcte
- Regarder les logs d'erreur en bas

### Le formulaire est désactivé ?
- Peut-être qu'une commande est en cours (`isSubmitting`)
- Attendre la fin du traitement
- Ou actualiser la page

---

## 📊 Logs Format

Les logs reçus de l'API ont ce format :

```typescript
interface SerialLog {
  timestamp: string;                                    // HH:MM:SS
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;                                      // Contenu du message
}
```

**Exemple :**
```json
{
  "timestamp": "14:35:42",
  "type": "command",
  "message": "HOME"
}
```

---

## 💡 Conseils d'Utilisation

1. **Tester les commandes simples d'abord :**
   ```
   GET_STATUS → affiche l'état complet
   ```

2. **Puis essayer des commandes contrôlées :**
   ```
   SET_FREQ:0.5 → change la fréquence
   SET_SPEED:50 → change la vitesse
   ```

3. **Enfin, les commandes d'action :**
   ```
   HOME → position initiale
   START → lancer le mouvement
   STOP → arrêter
   ```

4. **Filtrer les logs au besoin :**
   - Sélectionner "Commands" pour voir uniquement ce qu'on envoie
   - Sélectionner "Responses" pour voir uniquement les réponses

---

## 📚 Fichiers de Référence

- `src/app/components/SerialMonitor.tsx` - Composant amélioré
- `src/app/App.tsx` - Intégration dans l'app principale
- `src/app/hooks/useMachineController.ts` - Hook d'API

---

**Créé** : 2026-05-07  
**Status** : ✅ Implémenté et intégré  
**Test Recommandé** : Envoyer `GET_STATUS` via l'input du moniteur
