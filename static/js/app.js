const webview = window.chrome?.webview;
const $ = (id) => document.getElementById(id);

const i18n = {
  ja: {
    tagline: "環境とデータに合わせて、7-Zipを自動最適化", autoProfile: "AUTO PROFILE",
    headline: "迷わず、速く、安全に圧縮。", intro: "CPU・メモリ・ストレージ・ファイル構成を分析し、インストール済みの7-Zipへ最適な設定を渡します。",
    mode: "選択モード", waitingAnalysis: "分析待ち", sourceAndOutput: "対象と出力先", inputFolder: "圧縮するフォルダ／ドライブ",
    outputArchive: "出力アーカイブ", browse: "参照", detectedProfile: "検出プロファイル", storage: "ストレージ",
    logicalProcessors: "論理プロセッサ", memory: "メモリ", files: "ファイル",
    autoSettings: "自動設定", compressionLevel: "圧縮レベル", threads: "スレッド", solid: "ソリッド圧縮", sortType: "種類順ソート",
    autoExplanation: "分析結果に応じて設定理由を表示します。", securityAndSplit: "暗号化と分割",
    encrypt: "AES-256暗号化", encryptHelp: "パスワードをコマンドラインへ表示しません", password: "パスワード", confirmPassword: "パスワード確認",
    encryptFileNames: "ファイル名も暗号化", splitVolume: "分割ボリューム", noSplit: "分割しない", splitHelp: "分割は高速化ではなく、搬送・保存制限のための機能です。",
    start: "圧縮を開始", cancel: "中止", ready: "準備完了", resultRatio: "圧縮後", elapsed: "経過", independent: "7-Zipとは独立したアプリケーションです",
    analyzing: "ファイル構成を分析中…", compressing: "7-Zipで圧縮中…", completed: "圧縮が完了しました", cancelled: "処理を中止しました",
    on: "有効", off: "無効", jobs: "ジョブ", fast: "高速", balanced: "バランス", hybrid: "形式別", passwordMismatch: "パスワードが一致しません。",
    selectPaths: "入力フォルダと出力先を指定してください。", reanalyze: "入力が変わりました。もう一度分析してください。", engineMissing: "7-Zipが見つかりません", engineOld: "7-Zip 26.02以降へ更新してください",
    output_exists: "出力ファイルが既に存在します。別の名前を指定してください。", output_inside_input: "出力先を圧縮対象フォルダの外に指定してください。",
    output_directory_missing: "出力先フォルダが存在しません。", input_missing: "入力パスが存在しません。", password_empty: "パスワードを入力してください。"
  },
  en: {
    tagline: "Automatically tunes installed 7-Zip for your data and PC", autoProfile: "AUTO PROFILE",
    headline: "Fast, safe compression without guesswork.", intro: "Quick7Zip analyzes CPU, memory, storage and file layout, then selects practical options for the installed 7-Zip.",
    mode: "Selected mode", waitingAnalysis: "Waiting for analysis", sourceAndOutput: "Source and destination", inputFolder: "Folder or drive to archive",
    outputArchive: "Output archive", browse: "Browse", detectedProfile: "Detected profile", storage: "Storage",
    logicalProcessors: "logical processors", memory: "Memory", files: "Files",
    autoSettings: "Automatic settings", compressionLevel: "Compression level", threads: "Threads", solid: "Solid compression", sortType: "Sort by type",
    autoExplanation: "The reason for each setting appears after analysis.", securityAndSplit: "Encryption and splitting",
    encrypt: "AES-256 encryption", encryptHelp: "The password is not exposed in the command line", password: "Password", confirmPassword: "Confirm password",
    encryptFileNames: "Encrypt file names", splitVolume: "Split volumes", noSplit: "Do not split", splitHelp: "Splitting is for transport and storage limits; it does not improve speed.",
    start: "Start compression", cancel: "Cancel", ready: "Ready", resultRatio: "Result", elapsed: "Elapsed", independent: "Independent application; not affiliated with 7-Zip",
    analyzing: "Analyzing file layout…", compressing: "Compressing with 7-Zip…", completed: "Compression completed", cancelled: "Operation cancelled",
    on: "On", off: "Off", jobs: "jobs", fast: "Fast", balanced: "Balanced", hybrid: "Hybrid", passwordMismatch: "Passwords do not match.",
    selectPaths: "Select an input folder and output archive.", reanalyze: "The input changed. Analyze it again.", engineMissing: "7-Zip was not found", engineOld: "Update to 7-Zip 26.02 or newer",
    output_exists: "The output already exists. Choose a new file name.", output_inside_input: "Choose an output location outside the input folder.",
    output_directory_missing: "The output directory does not exist.", input_missing: "The input path does not exist.", password_empty: "Enter a password."
  }
};

let language = "ja";
try { language = localStorage.getItem("quick7zip-language") || "ja"; } catch (_) { /* NavigateToString may have an opaque origin. */ }
let analyzedPath = "";
let engineFound = false;
let busy = false;
let archiveStartTime = 0;
let archiveElapsed = 0;
let archiveTimer = null;

function t(key) { return i18n[language][key] || key; }
function post(message) { webview?.postMessage(message); }
function formatBytes(bytes) {
  const units = ["B", "KiB", "MiB", "GiB", "TiB", "PiB"];
  let value = Number(bytes || 0), unit = 0;
  while (value >= 1024 && unit < units.length - 1) { value /= 1024; unit++; }
  return `${value.toFixed(unit ? 1 : 0)} ${units[unit]}`;
}
function formatNumber(value) { return new Intl.NumberFormat(language === "ja" ? "ja-JP" : "en-US").format(value || 0); }

function renderElapsed() {
  $("elapsedText").textContent = archiveStartTime ? `${t("elapsed")} ${archiveElapsed.toFixed(2)}s` : `${t("elapsed")} —`;
}

function startElapsedTimer() {
  if (archiveTimer !== null) clearInterval(archiveTimer);
  archiveStartTime = performance.now();
  archiveElapsed = 0;
  const tick = () => {
    archiveElapsed = (performance.now() - archiveStartTime) / 1000;
    renderElapsed();
  };
  tick();
  archiveTimer = setInterval(tick, 200);
}

function stopElapsedTimer(keepResult = true) {
  if (archiveStartTime) archiveElapsed = (performance.now() - archiveStartTime) / 1000;
  if (archiveTimer !== null) clearInterval(archiveTimer);
  archiveTimer = null;
  if (!keepResult) { archiveStartTime = 0; archiveElapsed = 0; }
  renderElapsed();
}

function applyLanguage() {
  document.documentElement.lang = language;
  document.querySelectorAll("[data-i18n]").forEach((element) => element.textContent = t(element.dataset.i18n));
  document.querySelectorAll("[data-i18n-placeholder]").forEach((element) => element.placeholder = t(element.dataset.i18nPlaceholder));
  $("languageButton").textContent = language === "ja" ? "English" : "日本語";
  renderElapsed();
}

function updateStartState() {
  const input = $("inputPath").value.trim();
  const output = $("outputPath").value.trim();
  $("startButton").disabled = busy || !engineFound || !analyzedPath || input !== analyzedPath || !output;
}

function setBusy(value, statusKey) {
  busy = value;
  updateStartState();
  $("cancelButton").classList.toggle("hidden", !value);
  if (statusKey) $("statusText").textContent = t(statusKey);
}

function appendLog(text) {
  const log = $("logOutput");
  log.classList.add("visible");
  log.textContent += text.replace(/\r/g, "\n");
  if (log.textContent.length > 30000) log.textContent = log.textContent.slice(-30000);
  log.scrollTop = log.scrollHeight;
  const matches = [...text.matchAll(/(\d{1,3})%/g)];
  if (matches.length) {
    const percent = Math.min(100, Number(matches[matches.length - 1][1]));
    $("progressBar").style.width = `${percent}%`;
    $("progressText").textContent = `${percent}%`;
  }
}

function updateAnalysis(data) {
  analyzedPath = data.path;
  $("analysisNote").classList.remove("hidden");
  $("driveValue").textContent = data.drive;
  $("driveRoot").textContent = data.driveRoot || "—";
  $("cpuValue").textContent = data.cpu;
  $("memoryValue").textContent = formatBytes(data.memory);
  $("filesValue").textContent = formatNumber(data.files);
  $("sizeValue").textContent = formatBytes(data.bytes);
  $("levelValue").textContent = data.hybrid ? `${t("hybrid")} · mx=${data.level} + Copy` : `mx=${data.level}`;
  $("threadsValue").textContent = data.threads;
  $("solidValue").textContent = data.solid ? t("on") : t("off");
  $("sortValue").textContent = data.sortByType ? t("on") : t("off");
  const smallPercent = data.files ? Math.round(data.smallFiles * 100 / data.files) : 0;
  const storedPercent = data.bytes ? Math.round(data.compressedBytes * 100 / data.bytes) : 0;
  $("analysisNote").textContent = `${smallPercent}% small · ${storedPercent}% Copy · ${data.inaccessible} inaccessible · ${data.reparsePoints} reparse points`;
  $("planReason").textContent = data.reason;
  updateStartState();
}

webview?.addEventListener("message", ({data}) => {
  switch (data.type) {
    case "initialized":
      engineFound = data.found && data.supported;
      $("engineBadge").textContent = !data.found ? t("engineMissing") : (data.supported ? `7-Zip ${data.version}` : t("engineOld"));
      $("engineBadge").className = `badge ${engineFound ? "ok" : "error"}`;
      updateStartState();
      break;
    case "input_selected":
      $("inputPath").value = data.path; analyzedPath = ""; $("startButton").disabled = true;
      $("analysisNote").classList.add("hidden"); $("analysisNote").textContent = "";
      post({type: "analyze", path: data.path});
      if (!$("outputPath").value) $("outputPath").value = `${data.path.replace(/[\\/]$/, "")}.7z`;
      break;
    case "output_selected": $("outputPath").value = data.path; updateStartState(); break;
    case "analysis_started": setBusy(true, "analyzing"); break;
    case "analysis_progress": $("analysisNote").classList.remove("hidden"); $("analysisNote").textContent = data.status; break;
    case "analysis_complete": updateAnalysis(data); setBusy(false, "ready"); break;
    case "archive_started":
      $("logOutput").textContent = ""; $("progressBar").style.width = "0"; $("progressText").textContent = "0%";
      if (!archiveStartTime) startElapsedTimer();
      $("password").value = ""; $("passwordConfirm").value = ""; setBusy(true, "compressing"); break;
    case "archive_output": appendLog(data.text); break;
    case "archive_finished":
      stopElapsedTimer(true);
      setBusy(false, data.cancelled ? "cancelled" : (data.exitCode === 0 ? "completed" : "ready"));
      if (!data.cancelled && data.exitCode === 0) {
        $("progressBar").style.width = "100%";
        const ratio = data.inputBytes ? (data.outputBytes * 100 / data.inputBytes).toFixed(1) : "—";
        $("progressText").textContent = `${t("resultRatio")} ${ratio}% · ${formatBytes(data.outputBytes)}`;
      }
      if (data.exitCode !== 0 && !data.cancelled) appendLog(`\n7-Zip exit code: ${data.exitCode}\n`);
      break;
    case "cancelled": stopElapsedTimer(true); setBusy(false, "cancelled"); break;
    case "error":
      stopElapsedTimer(false);
      setBusy(false, "ready");
      alert(data.message === "reanalyze_required" ? t("reanalyze") : t(data.message));
      break;
  }
});

$("languageButton").addEventListener("click", () => {
  language = language === "ja" ? "en" : "ja";
  try { localStorage.setItem("quick7zip-language", language); } catch (_) { /* Optional preference storage. */ }
  applyLanguage();
});
$("browseInput").addEventListener("click", () => post({type: "browse_input"}));
$("browseOutput").addEventListener("click", () => post({type: "browse_output"}));
$("inputPath").addEventListener("input", () => { if ($("inputPath").value !== analyzedPath) analyzedPath = ""; updateStartState(); });
$("outputPath").addEventListener("input", updateStartState);
function requestAnalysis() {
  const path = $("inputPath").value.trim();
  if (!path || busy || path === analyzedPath) return;
  post({type: "analyze", path});
}
$("inputPath").addEventListener("change", requestAnalysis);
$("inputPath").addEventListener("keydown", (event) => {
  if (event.key === "Enter") { event.preventDefault(); requestAnalysis(); }
});
$("encryptToggle").addEventListener("change", () => $("passwordArea").classList.toggle("hidden", !$("encryptToggle").checked));
document.querySelectorAll(".password-visibility").forEach((button) => {
  button.addEventListener("click", () => {
    const input = $(button.dataset.target);
    const visible = input.type === "text";
    input.type = visible ? "password" : "text";
    button.textContent = visible ? "表示" : "隠す";
    button.setAttribute("aria-label", visible ? "パスワードを表示" : "パスワードを隠す");
    input.focus();
  });
});
$("startButton").addEventListener("click", () => {
  const input = $("inputPath").value.trim(), output = $("outputPath").value.trim();
  if (!input || !output) return alert(t("selectPaths"));
  if (input !== analyzedPath) return alert(t("reanalyze"));
  const encrypt = $("encryptToggle").checked;
  if (encrypt && ($("password").value !== $("passwordConfirm").value || !$("password").value)) return alert(t("passwordMismatch"));
  startElapsedTimer();
  post({type: "start_archive", input, output, encrypt, encryptHeaders: $("encryptHeaders").checked, password: $("password").value, split: $("splitSize").value});
});
$("cancelButton").addEventListener("click", () => post({type: "cancel"}));

applyLanguage();
post({type: "initialize"});
