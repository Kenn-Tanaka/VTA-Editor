# Variation Tree Analysis Editor - AGENTS.md

## 1. Project Overview (プロジェクト概要)
本プロジェクトは、バリエーションツリー分析（VTA: Variation Tree Analysis）を支援するフロントエンド完結型のWebアプリケーションである。
ユーザーが入力した事故シナリオテキスト（またはファイル）をLLMで構造化データ（JSON）に変換し、React Flowを用いて図解する。また、ユーザーが図上で対策（排除ノード・ブレイク）を付与した結果や補足説明を元に、LLMが「改善された業務フロー」と分析レポートを生成する。

## 2. Technology Stack & Vibe (技術スタックと開発思想)
- **Frontend**: React, TypeScript, Vite
- **State Management**: Zustand
- **Diagram UI**: `@xyflow/react` (React Flow)
- **Auto Layout**: 時間ランク判定 (`computeTimeRanks`) + 時間軸BT自動整列
- **Styling**: Tailwind CSS, Lucide Icons
- **Document Ingestion**: `pdfjs-dist`, `mammoth`, `xlsx`
- **Vibe**: 
  - ブラウザ上で完結するセキュアなSPA（Single Page Application）。
  - バックエンドサーバーは持たず、LLMとの通信はすべてクライアントサイドから直接「Lightweight LLM Gateway」のローカルエンドポイント（またはViteプロキシ `/v1`）へ送る。

## 3. Directory Structure (ディレクトリ構成規則)
- `src/components/`: 汎用UIコンポーネント（Header, ControlPanel, ModelSelectorModalなど）
- `src/features/vta/components/`: VTAキャンバス、カスタムノード・エッジ、補足説明パネル、モーダル類
- `src/features/vta/store/`: Zustandストア（`vtaStore.ts`）
- `src/features/vta/utils/`: レイアウト計算（`layout.ts`）、SVG出力（`svgExport.ts`）、ファイル解析（`fileIngestion.ts`）
- `src/services/llm/`: Lightweight LLM Gateway通信ロジック、プロンプト定義
- `src/types/`: VTAノード・エッジ、JSONスキーマ定義（重要）
- `src/features/vta/data/`: モデルカタログ、サンプルデータ
- `docs/`: マニュアル類（INSTALL_MANUAL / QUICKSTART / REFERENCE / BUILDER_MANUAL / LLM_GATEWAY_MANUAL）
- `scripts/`: 起動・パッケージ用スクリプト（`start-all.js`, `package.ps1`）
- `builder/`: 配布パッケージのビルド一式（`build.ps1`）。成果物は `Release/` に出力
- `tools/`: C++製ツールのソース（`llm-gateway`, `llm_test_cli`, `vta_starter`）
- `Release/`: 配布zip出力先（ルート直下に生成物の `.exe` / `.dll` / `.zip` を置かない）
- ルート直下の `gateway.json` は個人設定のためgit管理外。公開用の雛形は `gateway.example.json`

## 4. LLM Gateway Integration & Async Rules (LLM連携・非同期制御ルール)
- **APIキーのハードコード禁止**: フロントエンドコード内にAPIキーを絶対に含めない。
- **エンドポイント**: クライアントからのリクエスト先は `/v1`（Gatewayローカルプロキシ）とする。
- **認証ヘッダー**: ユーザー指定のGateway Tokenを用い、`Authorization: Bearer <Gateway Token>` を付与する。
- **タイムアウト設定**: モデルやプロバイダに応じて処理時間が変動するため、UI上でタイムアウト時間（デフォルト180秒）を変更可能とする。
- **中断（AbortController）の実装**: 長時間かかる推論や不要になったリクエストは、ユーザー操作（中断ボタン）で即座にキャンセル可能とし、UIやストア状態を安全にリセットする。

## 5. VTA Domain & Visual Rules (VTA図の視覚・描画要件)
- **時間軸方向**: 時間は下から上に向かって経過するレイアウト（BT: Bottom-to-Top）とする。頂上が最終的な不具合事象となる。
- **時系列順の垂直整列**: 全レーンを通して、各ノードの高さ（Y座標）は時系列順に整列させる。前後関係が特定できない・同時刻のノードは全レーン共通の同一高さ（同一タイムスライス）に配置する。
- **スイムレーン**: 横軸は当事者（企業、部署、担当者など）ごとに分割して配置する。
- **ノードのスタイル**: 通常作業は「細線」、変動要因は「太枠線（強調色）」で囲んだカードUIとする。
- **エッジ配線ルール**:
  - **同一レーン内の事象遷移**: 下のノードの上辺（Top Handle）と上のノードの底辺（Bottom Handle）を垂直直線で結ぶ。
  - **レーン間の情報伝達（一方向 `──▶` / 双方向 `◀──▶`）**: ノードの左右いずれかの辺（Left/Right Handle）から相手ノードの左右いずれかの辺（Left/Right Handle）を直交ステップ配線で結ぶ。
  - **配線の重なり防止**: 線がノードカードの下に隠れないよう、垂直スペース（`VERTICAL_GAP=70px`）およびレーン間スペースをコリドーとして配線する。
- **対策ポイントの表現**:
  - 排除ノード: 該当ノードの右上隅に「○印」バッジを表示する。
  - ブレイク: エッジを遮断する「BREAK」バッジと遮断線で表現する。
- **用語の統一**: 「理想業務フロー」ではなく、現実的な安全工学の観点から**「改善された業務フロー」**という用語を使用する。
- **事実抽出の厳格性（解釈・推測の完全禁止）**:
  - ノード（カード）および補足説明には、**入力テキストに書かれている事実のみ**を記述する。LLMによる勝手な解釈、推論、心理状態の推測、原因補完は完全禁止とする。

## 6. UI/UX & Canvas Design Rules (UI設計・重なり防止)
- **操作ボタンの保護**: ツールバーや重要ボタンの上に固定カード（凡例やステータス）を重ねて配置しない。凡例やサブ情報はツールバーからの「トグル式ポップオーバー」または「モーダル」として表示する。
- **モーダル管理**: ダイアログは `createPortal` でルートに描画し、Escapeキーやオーバーレイクリックで閉じられるようにする。

## 7. Vector (SVG) Export Rules (SVG出力の互換性)
- **標準SVG要素の使用**: WordやPowerPoint等での表示崩れ・欠落を防ぐため、`<foreignObject>` は使用せず、標準の `<text>` および `<tspan>` によるテキスト折り返しで出力する。
- **空レーン制御**: ノードが存在しない空レーンを除外してコンパクトに出力するオプションと、全レーン出力オプションを選択可能とする。

## 8. Testing & Execution (検証とビルド)
- 開発サーバー起動: `npm run dev`
- コード変更後は必ず `npm run build`（TypeScript型検査 + Viteビルド）を実行し、型エラーや未インポートが存在しないことを検証すること。

## 9. Binary & Packaging Rules (バイナリの静的リンクと配布パッケージ規則)
- **C++実行バイナリの静的リンク（完全必須）**:
  - `llm-gateway.exe`、`llm-gateway-credential-manager.exe`、`llm_test.exe` 等のC++実行ファイルは、ランタイムDLL（`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`）への依存を排除するため、**必ず `-static -static-libgcc -static-libstdc++` による静的リンクでビルドする**。
  - MSVC環境でビルドする場合はランタイムライブラリに `/MT`（マルチスレッド静的リンク）を指定すること。
- **依存DLLの完全同梱**:
  - OpenSSL等の動的ライブラリ（`libcrypto-3-x64__.dll`, `libssl-3-x64__.dll`, `zlib1.dll`, `zlib1__.dll`）を必要とするバイナリは、配布パッケージ（zip）内の実行ファイルと同階層に必ず同梱し、C++ランタイムや開発環境が未インストールのクリーンなWindows環境でも即使えるように構成する。

