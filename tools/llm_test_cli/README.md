# Lightweight LLM Gateway C++ CLI 接続テストツール

Lightweight LLM Gateway (127.0.0.1:8765) の疎通確認および推論テストをコマンドラインから即座に実行できるC++製ツールです。

## 特徴
- **外部依存ライブラリ不要**: Windows標準の `WinHTTP` API を利用しているため、Windows環境で即座にビルド・実行可能。
- **2段階診断**:
  1. `GET /v1/models` によるモデル一覧取得・疎通テスト
  2. `POST /v1/chat/completions` による実際のLLM推論テスト・レイテンシ測定
- **詳細なエラーハンドリング**: 接続拒否(12029)、認証エラー(401)、Allowlist拒否(400/403)などを分かりやすく診断表示。

---

## 実行方法

### 1. ビルド
付属の `build.bat` を実行するか、以下のコマンドでコンパイルします。
```bash
g++ -O2 -std=c++17 main.cpp -lwinhttp -o llm_test.exe
```

### 2. 使用方法

```bash
# ヘルプ表示
.\llm_test.exe --help

# 1. モデル一覧取得テスト (GET /v1/models)
.\llm_test.exe --models

# 2. OpenRouter のモデルで推論テスト
.\llm_test.exe --model openrouter/anthropic/claude-sonnet-5

# 3. LM Studio (ローカル) のモデルで推論テスト
.\llm_test.exe --model lmstudio/local-model --prompt "こんにちは"

# 4. Gateway Client Token が必要な場合
.\llm_test.exe --token <あなたのGatewayToken> --model openrouter/openai/gpt-4o
```
