# Variation Tree Analysis Editor

バリエーションツリー分析（VTA: Variation Tree Analysis）をブラウザ上で直感的に実施できる、フロントエンド完結型のWebアプリケーションです。

事故・トラブルの時系列シナリオテキスト（またはPDF/Word/Excel文書）から、LLMを活用して構造化データ（JSON）を自動生成し、時間軸が下から上（BT: Bottom-to-Top）へ進むスイムレーン図として可視化します。図上で対策（排除ノード・ブレイク）を指定することで、AIが再発防止策を論理的に組み込んだ「改善された業務フロー」と「分析レポート」を生成します。

---

## 1. 主要機能

### 1.1 VTA描画エンジン
- **時間軸方向（Bottom-to-Top）**: 時間は下から上に向かって進行し、頂上が最終的な不具合事象となります。
- **時系列ランク整列**: レーンを横断して、同一時刻・同順序のノードは同一水平スライスに整列配置されます。
- **スイムレーン形式**: 横軸は当事者（部門、担当者、設備など）ごとに分割配置されます。
- **直交ステップ配線**: レーン間の情報伝達線は、ノードカードを回避する5セグメント直交ステップ配線により重なりを防止します。
- **一方向／双方向の伝達表現**: レーン間通信の一方向矢印（`──▶`）と相互確認・双方向矢印（`◀──▶`）を自動判定・手動切替可能。

### 1.2 コア編集 & 信頼性機能
- **Undo / Redo（履歴管理）**: ノードやエッジの追加・編集・削除、対策付与、補足説明変更、ノード手動ドラッグ配置などの操作を最大30世代まで取り消し・やり直し可能。
- **手動移動座標の保護・永続化**: キャンバス上でノードをドラッグして微調整した座標は、ノード追加・編集・削除・Undo/Redoを行っても確実に保持され、プロジェクトファイル（.json）への保存・読込でも完全に復元されます（「BT自動整列」ボタン押下時のみ全体再計算）。
- **未保存データ保護**: 編集中の作業データがある場合、ヘッダーに未保存バッジが表示され、ブラウザタブの離脱・リロード時には警告ダイアログ（`beforeunload`）で意図せぬデータ消失を防ぎます。
- **ドキュメント直接取込**: `.txt`, `.md`, `.docx`, `.pdf`, `.xlsx`, `.json` ファイルをドラッグ＆ドロップして瞬時にシナリオテキスト化。

### 1.3 AI推論 & 対策支援
- **最新モデル対応**: Claude 3.7 Sonnet, DeepSeek R1, Gemini 2.0 Flash, GPT-4o などの実在モデルに対応。
- **AI対策提案**: VTA図内の潜在的な変動要因や伝達経路から、AIが排除ノード候補（○印）とブレイク遮断箇所（BREAK）を論理的に推薦。
- **改善された業務フロー & レポート生成**: 付与された対策に基づき、マークダウン形式で整形された「改善された業務フロー」と「論理的分析レポート」を自動再生成。

### 1.4 高互換ベクターSVGエクスポート
- WordやPowerPoint等での表示崩れを防ぐため、`<foreignObject>` を一切使用せず、標準の `<text>` および `<tspan>` による全角・半角文字数換算ベースの精密なテキスト自動折り返しでベクターSVGを出力します。
- 補足説明一覧の同時出力や、空レーンの除外オプションを完備。

---

## 2. キーボードショートカット一覧

| ショートカット | 機能 | 説明 |
| :--- | :--- | :--- |
| `Ctrl + Z` | **元に戻す (Undo)** | 直前の操作（カード追加・編集・削除・対策付与等）を取り消します。 |
| `Ctrl + Y` / `Ctrl + Shift + Z` | **やり直す (Redo)** | 取り消した操作を再び適用します。 |
| `Ctrl + S` | **プロジェクト保存** | 現在の作業セッション（シナリオ・VTA図・補足説明・レポート）をJSON保存します。 |
| `Ctrl + O` | **プロジェクト読込** | 保存済みのVTAプロジェクトファイル（.json）を開きます。 |
| `Ctrl + E` | **SVG出力** | VTAツリー図＋補足説明のエクスポートモーダルを開きます。 |
| `Delete` / `Backspace` | **ノード・エッジ削除** | 選択中のカードまたは接続線を削除します。 |

---

## 3. クイックスタート

### 3.1 スタンドアロン実行（配布パッケージ）
配布パッケージ（`vta-analysis-package.zip`）を展開後：
1. `start.bat` または `vta-starter.exe` をダブルクリックして起動します。
2. ブラウザが自動的に開き、Webエディタが利用可能になります。
3. 初回起動時は `credential-manager.bat` から OpenRouter 等の API キーを登録してください。

### 3.2 開発環境でのセットアップ

#### 前提条件
- Node.js 18+
- npm 9+

#### インストール & 開発サーバー起動
```bash
# 依存ライブラリのインストール
npm install

# 開発サーバー起動 (ポート5173)
npm run dev
```

#### 自動テストの実行 (Vitest)
```bash
# ユニットテストの実行
npm test

# ウォッチモードでテスト実行
npm run test:watch
```

#### 本番バンドルのビルド
```bash
npm run build
```
`dist/` ディレクトリに最適化された本番用SPAバンドルが出力されます。

---

## 4. プロジェクト構成

```text
VTA/
├── src/
│   ├── components/                 # 汎用UIコンポーネント
│   │   ├── Header.tsx              # グローバルヘッダー (保存・読込・SVG出力・Undo/Redo)
│   │   ├── ControlPanel.tsx        # コントロールパネル親コンテナ
│   │   ├── control-panel/          # 機能別サブコンポーネント (Phase 5)
│   │   │   ├── LLMConfigSection.tsx       # LLM Gateway接続設定
│   │   │   ├── ScenarioInputSection.tsx   # シナリオ入力・文書解析・VTA抽出
│   │   │   ├── MeasuresStatusSection.tsx  # 対策適用状況・AI対策提案・シナリオ再生成
│   │   │   ├── ResultsSection.tsx         # 改善フロー・レポート表示 (Markdown)
│   │   │   └── DebugLogsSection.tsx       # 実行・通信ログパネル
│   │   └── ModelSelectorModal.tsx  # モデル検索・選択ダイアログ
│   ├── features/vta/
│   │   ├── components/             # VTAダイアグラム描画コンポーネント
│   │   │   ├── VTACanvas.tsx       # React Flowキャンバス & ツールバー
│   │   │   ├── VTANode.tsx         # VTAカード (上下左右分離Handle)
│   │   │   ├── VTAEdge.tsx         # 5セグメント直交配線エッジ (一方向/双方向/BREAK)
│   │   │   ├── SwimlaneBackground.tsx # 部門別スイムレーン背景
│   │   │   ├── NodeEditorModal.tsx # カード編集・新規追加モーダル
│   │   │   ├── SupplementaryNotesPanel.tsx # 補足説明右サイドパネル
│   │   │   └── SVGExportModal.tsx  # SVG出力詳細設定モーダル
│   │   ├── store/
│   │   │   ├── vtaStore.ts         # Zustand 5 状態管理 (Undo/Redo, Dirty管理)
│   │   │   └── vtaStore.test.ts    # ストア機能ユニットテスト
│   │   └── utils/
│   │       ├── layout.ts           # BT時系列ランク整列 & 障害物回避配線アルゴリズム
│   │       ├── layout.test.ts      # レイアウトエンジン・テスト
│   │       ├── svgExport.ts        # Word/PowerPoint完全互換SVG生成エンジン
│   │       └── svgExport.test.ts   # SVGエクスポート・テスト
│   └── services/llm/
│       ├── client.ts               # LLM Gateway通信クライアント
│       ├── prompts.ts              # 外部プロンプト読み込み & パラメータ置換
│       └── prompts.test.ts         # プロンプト置換・テスト
├── public/
│   └── prompts.json                # 外部化されたLLMプロンプト定義
├── vta-analysis-package/           # 配布用スタンドアロンパッケージ
│   ├── llm-gateway.exe             # C++17 静的リンク LLM Gateway
│   ├── vta-starter.exe             # ワンクリックランチャー
│   ├── dist/                       # ビルド済みフロントエンド成果物
│   └── start.bat                   # 起動バッチ
└── package.json
```

---

## 5. ライセンス & 分析手法
- **分析手法**: バリエーションツリー分析（VTA: Variation Tree Analysis、ヒューマンファクタ分析手法）
- **ライセンス**: MIT License
