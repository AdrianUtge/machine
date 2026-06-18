# 02 — Frontend (React + TypeScript + Vite + Tailwind)

Dossier : `frontend/`. Point d'entrée : `src/main.tsx` → `src/app/App.tsx`.

## Stack

- **React + TypeScript**, build **Vite** (`vite.config.ts`).
- **Tailwind** (via `@tailwindcss/vite`) + composants **shadcn/ui** (`components/ui/`).
- Icônes **lucide-react**. Pas de `tsconfig.json` : Vite gère le TS.
- Appels réseau : `fetch` natif vers `http://localhost:8000/api` (constante
  `API_BASE` dans `useMachineController.ts`).

## Arborescence utile

```
frontend/src/app/
├── App.tsx                     # racine : aiguillage écrans + layout
├── hooks/
│   └── useMachineController.ts # ★ état + TOUS les appels API (unique point réseau)
└── components/
    ├── ConnectionScreen.tsx    # choix du port/interface + Connect
    ├── MotionControl.tsx       # fréquence, force (globale/par cellule), presets
    ├── StatusPanelSimple.tsx   # statut machine + latence + torque on/off
    ├── PositionsAndSensors.tsx # 4 positions + 4 capteurs, bouton GOTO
    ├── ForceGraph.tsx          # graphe temps réel d'une cellule
    ├── SerialMonitor.tsx       # moniteur série (mode Advanced)
    ├── figma/                  # ImageWithFallback
    └── ui/                     # primitives shadcn (génériques, peu à toucher)
```

## Le hook central : `useMachineController.ts`

C'est le **cœur** du frontend. Tout passe par lui ; les composants ne font
jamais de `fetch` eux-mêmes.

Expose : `isConnected`, `machineState`, `logs`, `error`, `latencyMs`,
`connect/disconnect`, commandes (`home/start/stop/setFrequency/setForce/goto/
torqueOn/torqueOff/sendManualCommand`), presets (`customPresets/savePreset/
deletePreset`), `refreshLogs/clearLogs`.

### Polling (deux boucles)

| Boucle              | Période | Endpoint        | But                               |
|---------------------|---------|-----------------|-----------------------------------|
| Statut              | 200 ms  | `GET /api/status` | positions/forces/état temps réel |
| Logs                | 500 ms  | `GET /api/logs`   | moniteur série (volume réduit)   |

La **latence** affichée est l'aller-retour de `GET /api/status`, lissé en EMA
(`prev*0.7 + sample*0.3`). `latencyMs = -1` signifie « lien coupé ».

### Type `MachineState` (doit rester aligné sur le backend)

Correspond à `MachineStateResponse` d'`api.py`. Si vous ajoutez un champ côté
backend, ajoutez-le ici aussi.

## Conventions

- **Aucune logique réseau hors du hook.** Un composant reçoit des props et des
  callbacks ; il ne connaît pas l'API.
- Mode **Advanced** (toggle dans `App.tsx`) : affiche le moniteur série + le
  réglage de force par cellule.
- Messages d'erreur explicites : `sendCommand` distingue 404 (route absente) et
  `Failed to fetch` (backend injoignable).

## Build & dev

```bash
cd frontend
npm install
npm run dev      # serveur Vite (HMR) : http://localhost:5173
npm run build    # build production -> dist/
```

`dist/` est servi tel quel en production (voir `run.sh`, `vite preview`).

## Composants supprimés (nettoyage)

`LoadCell.tsx`, `PositionControl.tsx`, `StatusPanel.tsx` et `src/imports/` ont
été retirés car non importés (morts). L'UI active utilise `StatusPanelSimple`,
`PositionsAndSensors`, `MotionControl`.
