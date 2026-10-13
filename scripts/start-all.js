import { spawn, exec, execSync } from 'node:child_process';
import { existsSync, createReadStream, statSync, readFileSync } from 'node:fs';
import { resolve, extname, join } from 'node:path';
import http from 'node:http';
import net from 'node:net';

const rootDir = process.cwd();
const configPath = resolve(rootDir, 'tools/llm-gateway/config/gateway.json');
const distDir = resolve(rootDir, 'dist');
const nodeModulesDir = resolve(rootDir, 'node_modules');

const PORT = 5173;
const GATEWAY_PORT = 8765;

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.mjs': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.ico': 'image/x-icon',
  '.wasm': 'application/wasm',
  '.txt': 'text/plain; charset=utf-8',
};

// Check if a port is already open
function checkPort(port, host = '127.0.0.1') {
  return new Promise((res) => {
    const socket = new net.Socket();
    socket.setTimeout(400);
    socket.on('connect', () => {
      socket.destroy();
      res(true);
    });
    socket.on('timeout', () => {
      socket.destroy();
      res(false);
    });
    socket.on('error', () => {
      socket.destroy();
      res(false);
    });
    socket.connect(port, host);
  });
}

// Candidate locations for llm-gateway binary
const candidatePaths = [
  resolve(rootDir, 'llm-gateway.exe'),
  resolve(rootDir, 'tools/llm-gateway/llm-gateway.exe'),
  resolve(rootDir, 'tools/llm-gateway/bin/llm-gateway.exe'),
  resolve(rootDir, 'tools/llm-gateway/build/llm-gateway.exe'),
  resolve(rootDir, 'tools/llm-gateway/build/bin/llm-gateway.exe'),
  resolve(rootDir, 'tools/llm-gateway/llm-gateway'),
];

let gatewayBin = candidatePaths.find((p) => existsSync(p));

if (!gatewayBin) {
  try {
    const whereOutput = execSync('where.exe llm-gateway 2>nul || where llm-gateway 2>nul', {
      encoding: 'utf-8',
    }).trim();
    if (whereOutput) {
      gatewayBin = whereOutput.split(/\r?\n/)[0].trim();
    }
  } catch {}
}

/**
 * Starts a 0-dependency standalone static HTTP server + /v1 proxy for the built dist/ folder.
 */
function startStandaloneServer() {
  return new Promise((resServer) => {
    const server = http.createServer((req, res) => {
      const url = new URL(req.url || '/', `http://${req.headers.host || '127.0.0.1'}`);
      const pathname = decodeURIComponent(url.pathname);

      // 1. Proxy /v1/* requests to LLM Gateway (127.0.0.1:8765)
      if (pathname.startsWith('/v1')) {
        const cleanHeaders = { ...req.headers };
        delete cleanHeaders['host'];
        delete cleanHeaders['accept-encoding'];
        delete cleanHeaders['content-length'];
        cleanHeaders['accept-encoding'] = 'identity'; // Request raw uncompressed stream to prevent zlib issues
        cleanHeaders['host'] = `127.0.0.1:${GATEWAY_PORT}`;

        const proxyReq = http.request(
          {
            hostname: '127.0.0.1',
            port: GATEWAY_PORT,
            path: pathname + url.search,
            method: req.method,
            headers: cleanHeaders,
          },
          (proxyRes) => {
            if (!res.headersSent) {
              res.writeHead(proxyRes.statusCode || 200, proxyRes.headers);
            }
            proxyRes.pipe(res);
            proxyRes.on('error', () => {
              if (!res.writableEnded) res.end();
            });
          }
        );

        proxyReq.on('error', (err) => {
          if (!res.headersSent) {
            res.writeHead(502, { 'Content-Type': 'application/json; charset=utf-8' });
            res.end(
              JSON.stringify({
                error: {
                  message: `LLM Gateway (127.0.0.1:${GATEWAY_PORT}) への接続に失敗しました: ${err.message}`,
                  type: 'gateway_connection_error',
                },
              })
            );
          } else if (!res.writableEnded) {
            res.end();
          }
        });

        req.on('aborted', () => {
          proxyReq.destroy();
        });

        res.on('close', () => {
          if (!proxyReq.destroyed) {
            proxyReq.destroy();
          }
        });

        req.pipe(proxyReq);
        return;
      }

      // 2. Serve static files from dist/
      let filePath = resolve(distDir, pathname.replace(/^\//, ''));

      // If directory or doesn't exist, check for index.html (SPA Fallback)
      if (existsSync(filePath) && statSync(filePath).isDirectory()) {
        filePath = resolve(filePath, 'index.html');
      }

      if (!existsSync(filePath)) {
        // SPA Fallback to dist/index.html
        filePath = resolve(distDir, 'index.html');
      }

      if (existsSync(filePath) && statSync(filePath).isFile()) {
        const ext = extname(filePath).toLowerCase();
        const contentType = MIME_TYPES[ext] || 'application/octet-stream';
        res.writeHead(200, {
          'Content-Type': contentType,
          'Access-Control-Allow-Origin': '*',
        });
        createReadStream(filePath).pipe(res);
      } else {
        res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
        res.end('404 Not Found - dist/index.html が見つかりません。');
      }
    });

    server.listen(PORT, '127.0.0.1', () => {
      resServer(server);
    });
  });
}

function openBrowser(url) {
  const startCmd =
    process.platform === 'win32'
      ? `start "" "${url}"`
      : process.platform === 'darwin'
      ? `open "${url}"`
      : `xdg-open "${url}"`;
  exec(startCmd, () => {});
}

async function main() {
  console.log('\x1b[34m================================================================\x1b[0m');
  console.log('\x1b[1m\x1b[36m   Variation Tree Analysis Editor & LLM Gateway Starter\x1b[0m');
  console.log('\x1b[34m================================================================\x1b[0m\n');

  let gatewayProcess = null;
  const isGatewayRunning = await checkPort(GATEWAY_PORT);

  if (isGatewayRunning) {
    console.log(`\x1b[32m✔ [LLM Gateway]\x1b[0m ポート ${GATEWAY_PORT} で稼働中の Gateway を検知しました。`);
  } else if (gatewayBin) {
    const activeConfig = existsSync(configPath)
      ? configPath
      : existsSync(resolve(rootDir, 'gateway.json'))
      ? resolve(rootDir, 'gateway.json')
      : null;

    console.log(`\x1b[32m✔ [LLM Gateway]\x1b[0m 実行バイナリ: ${gatewayBin}`);
    if (activeConfig) {
      console.log(`\x1b[32m✔ [LLM Gateway]\x1b[0m 設定ファイル: ${activeConfig}`);
    }
    console.log(`\x1b[36m▶ [LLM Gateway]\x1b[0m ポート ${GATEWAY_PORT} で Gateway を起動中...\n`);

    const args = activeConfig ? [activeConfig] : [];
    gatewayProcess = spawn(gatewayBin, args, {
      stdio: 'inherit',
      windowsHide: false,
    });

    gatewayProcess.on('error', (err) => {
      console.error(`\x1b[31m✖ [LLM Gateway エラー]\x1b[0m ${err.message}`);
    });

    gatewayProcess.on('exit', (code) => {
      if (code !== 0 && code !== null) {
        console.log(`\x1b[33mℹ [LLM Gateway]\x1b[0m プロセス終了 (code: ${code})`);
      }
    });

    // Short wait for gateway to bind
    await new Promise((r) => setTimeout(r, 600));
  } else {
    console.log(`\x1b[33m⚠ [LLM Gateway]\x1b[0m llm-gateway.exe が見つかりませんでした。`);
    console.log(`  配置先候補: tools/llm-gateway/llm-gateway.exe または root\n`);
  }

  // Check if dist/ exists (standalone ready)
  const isDistReady = existsSync(resolve(distDir, 'index.html'));
  const isViteReady = existsSync(resolve(nodeModulesDir, 'vite/bin/vite.js'));

  let webServer = null;
  let viteProcess = null;

  if (isDistReady) {
    console.log(`\x1b[32m✔ [VTA Web Editor]\x1b[0m 本番ビルド済みアセット (dist/) を検出しました。`);
    console.log(`\x1b[36m▶ [VTA Web Editor]\x1b[0m Webサーバー (http://127.0.0.1:${PORT}) を起動し、ブラウザを開きます...\n`);

    webServer = await startStandaloneServer();
    openBrowser(`http://127.0.0.1:${PORT}`);
  } else if (isViteReady) {
    console.log(`\x1b[36m▶ [VTA Web Editor]\x1b[0m Vite 開発サーバーを起動し、ブラウザを開きます...\n`);
    const viteJs = resolve(nodeModulesDir, 'vite/bin/vite.js');
    viteProcess = spawn(process.execPath, [viteJs, '--host', '127.0.0.1', '--port', String(PORT), '--open'], {
      stdio: ['ignore', 'inherit', 'inherit'],
    });
  } else {
    console.log(`\x1b[33m📦 [初期セットアップ]\x1b[0m 依存パッケージをインストールしています (npm install)...`);
    try {
      execSync('npm install', { stdio: 'inherit', cwd: rootDir });
      console.log(`\x1b[32m✔ [初期セットアップ]\x1b[0m パッケージインストール完了。Webエディタをビルドしています...`);
      execSync('npm run build', { stdio: 'inherit', cwd: rootDir });

      console.log(`\x1b[36m▶ [VTA Web Editor]\x1b[0m Webサーバー (http://127.0.0.1:${PORT}) を起動し、ブラウザを開きます...\n`);
      webServer = await startStandaloneServer();
      openBrowser(`http://127.0.0.1:${PORT}`);
    } catch (err) {
      console.error(`\x1b[31m✖ [セットアップエラー]\x1b[0m ${err.message}`);
    }
  }

  console.log(`\n\x1b[32m================================================================\x1b[0m`);
  console.log(`\x1b[32m  ✔ Variation Tree Analysis Editor が準備完了しました！\x1b[0m`);
  console.log(`  URL: \x1b[1m\x1b[36mhttp://127.0.0.1:${PORT}\x1b[0m`);
  console.log(`  終了するにはこのウィンドウで [Ctrl + C] を押してください。`);
  console.log(`\x1b[32m================================================================\x1b[0m\n`);

  // Graceful shutdown on Ctrl+C
  function cleanup() {
    console.log('\n\x1b[36m[終了処理]\x1b[0m プロセスを安全に停止しています...');
    if (gatewayProcess && !gatewayProcess.killed) {
      try {
        gatewayProcess.kill();
      } catch {}
    }
    if (viteProcess && !viteProcess.killed) {
      try {
        viteProcess.kill();
      } catch {}
    }
    if (webServer) {
      try {
        webServer.close();
      } catch {}
    }
    process.exit(0);
  }

  process.on('SIGINT', cleanup);
  process.on('SIGTERM', cleanup);
}

main();
