# work → main ベースライン検証（PR #7）

検証日: 2026-10-17 / UE 5.8.3 (58210709) / Windows Win64 Development。
対象: `main=1b5844c`、調査開始時`work=e758fb8`（17 commits ahead / 0 behind、43 files）。
PR: https://github.com/Soso2e/WEVORA/pull/7 。今回の修正は既存`work`上で行う。
最終結果・マージ判断は`PROGRESS.md`の2026-10-17欄を参照。

## 差分と現在の実装範囲

| 対象 | workの実装 / 検証の境界 |
|---|---|
| 移動 | 空中加速度分離、入力中の有料ホバー、無入力の自然落下、空中ジャンプ8 mana、残マナ比例ダッシュ。保存済みBlueprintで実World tick / sweep / 着地 / Ctrl優先 / 入力解除を自動検証。実入力・操作感はPIE対象 |
| 通常 / チャージ | LMB押下→解放でForward弾。最大2秒、Power最大2倍。共通0.5秒cooldown、予約なし。保存済みBlueprint解決後に倍率適用。既存SimpleAttackが状態・Power・競合入力・キャンセルを検証 |
| ジェスチャー | E保持→描画→解放で発射。Thrust / Sweep / Circle / Slam認識。描画中カメラ固定、フェードする仮トレイル。全Shapeの配送は共通Projectile。専用の範囲攻撃やFieldは未実装 |
| Fire | 基礎Power1で直撃25＋燃焼5 HP/s×3秒。実命中、燃焼終了、仮炎の開始/終了、衝突無効を既存テストで検証 |
| Wind | 基礎Power1で押出900＋Lift220 cm/s、直接ダメージなし。外力中はAIを休止、終了後再開。壁とのsweep、物理Body、汎用Actorの作用を既存テストで検証 |
| 属性反応 | 対象側ReactionRulesがElement / Shape / RequiredState / Powerを解決。Wind×Burningの追加作用はテスト内でルールを追加した例。標準データに常設された複合反応ではない。部位別反応は未実装 |
| Lightning | Q選択、Blueprint発射設定、紫の手元ライト、弾のメッシュ演出、単発25×Powerダメージ。今回、保存済み発射BPを通る通常/最大チャージの実sweep命中・HP・非燃焼・非変位・継続ダメージなしを検証するLightningHitを追加。感電・連鎖・専用配送は未実装。ジェスチャートレイルの色分岐はFire以外共通で水色 |
| 照準 / UI | 肩越し、RMB精密照準、カメラ→対象→発射位置の共通照準、遮蔽/至近壁の予告とSpawn禁止、速度継承はOpt-in。十字レティクル、チャージ/CTゲージ、頭上HP/燃焼表示。TPSAim / TPSCoverPreviewは照準論理と実命中を検証。ピクセル描画・日本語表示は未確認。円形レティクルは次開発対象 |
| VFX | 三属性のInstancedStaticMesh仮演出＋任意Niagara差替。命中面/表示メッシュの素材を引き継ぐ衝突なし破片、寿命/個数上限。AttackEffectsは構成・素材・飛散・弾消滅後の保持/破片の終了を検証。実RHIの見た目・Niagara差替は別途確認 |
| Blueprint | 既存プレイヤーBPの4関数でFOV / 魔法移動倍率 / 発射データ / 反動を編集。BlueprintRulesが保存済みoverrideとC++互換処理を照合。今回は移行/素材生成スクリプトを再実行しない |
| Config / plugins | work既存差分はAssetManagerのGameFeatureData登録、ローカルツールplugin有効化、無効AFSの設定値。今回上書きしない。Player向けの適合性はビルド・Cook・起動の結果で判断 |

## 既知の移動2件の原因

修正前`Saved/Automation/BaselineBefore/index.json`: 22件中20成功、2失敗。
診断`Saved/Automation/BaselineDiagnostic/index.json`: 移動7件中5成功、同2件失敗。

- 保存済みプレイヤーBPの巡航1000、ダッシュ加算1200、降下倍率0.9に対し、
  旧テストはC++初期値1200 / 1800 / 0.8を前提とした固定閾値。
  実測はダッシュ前の水平速度1000、低速からのダッシュ1502.5、
  自然落下-588に対してWASD時-529.2（30/60/120 fps共通、比率0.9）。
- BurstAndBrakeは高さ5000から2.4秒＋ダッシュ後0.5167秒落下し、
  Alt検証中に床へ着地（Z=92.15、Walking、垂直速度0）。
  そのためHovering / マナ切れ後の落下のassertが空中挙動を検証できていなかった。
- 修正: 読み込んだBlueprintの設定で加算量/上限/減衰/重力倍率を検証。
  開始高度10000、Altが落下を受け止める時間3秒、前提条件にIsFallingのassertを追加。
  許容誤差だけを広げたり、失敗assertを削除したりしていない。
- この2件についてゲームプレイ実装の修正は不要。保存済みアセット・調整値を維持。

## 再実行（PowerShell、リポジトリroot）

```powershell
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$project = (Resolve-Path .\WEVORA.uproject).Path
& "$engine\Build\BatchFiles\Build.bat" WEVORAEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoPCH -NoSharedPCH -NoUBA -UBANoDetour -nocache -MaxParallelActions=4
& "$engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project -unattended -nop4 -nosplash -nosound -NullRHI '-ExecCmds=Automation RunTests WEVORA.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Saved\Automation\BaselineFinal" "-abslog=$PWD\Saved\Logs\BaselineFinal.log"
& "$engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project -run=CompileAllBlueprints -unattended -nop4 -nosound -NullRHI "-abslog=$PWD\Saved\Logs\BaselineBlueprintCompile.log"
& "$engine\Build\BatchFiles\Build.bat" WEVORA Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoPCH -NoSharedPCH -NoUBA -UBANoDetour -nocache -MaxParallelActions=4
& "$engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$project" -nop4 -unattended -utf8output -platform=Win64 -clientconfig=Development -skipbuild -skipbuildeditor -nocompileeditor -cook '-map=/Game/ThirdPerson/Lvl_ThirdPerson' -stage -pak -package -archive "-archivedirectory=$PWD\Saved\BaselinePlayer"
$player = Start-Process "$PWD\Saved\BaselinePlayer\Windows\WEVORA\Binaries\Win64\WEVORA.exe" -ArgumentList @('-windowed', '-ResX=1280', '-ResY=720', '-unattended', '-nosound', '-benchmark', '-seconds=15', "-abslog=$PWD\Saved\Logs\BaselinePlayerRun.log") -PassThru -WindowStyle Hidden
$player.WaitForExit()
$player.ExitCode
git diff --check
git lfs fsck
```

Editor/CookerはプロジェクトDLLを使用するため、再ビルド前にそれらの処理が終了していること。
テスト実行プロセスの終了コード0だけでは成功としない。`index.json`の件数・state・warnings/errorsを確認する。
`Saved/`はGit管理外。共有可能な最終結果の要約はPROGRESSとPRに記録する。

## 人間が確認する項目（未実施）

1. PIEとパッケージ版の両方で`Lvl_ThirdPerson`を開始。マウス捕捉、WASD、地上/空中Space、Shift保持/解放、Alt、Ctrl優先、マナ枯渇→着地回復、終了→再開を確認。
2. LMB短押し・1秒・2秒以上のチャージ、ゲージと0.5秒CT、E単独の4Gesture、失敗入力、LMB/E競合、Shift/Ctrlキャンセル後の解放で誤射しないこと。
3. QでFire→Wind→Lightning。初期HP100の敵へ通常Fireで75→燃焼終了60、WindでHPを減らさず押出/Lift、Lightning通常で75・最大チャージで50、その後追加ダメージなし。各比較は新しい敵/再開後に行う。
4. 移動・ホバー中の命中、RMBのFOV変化と復帰、遮蔽時の赤い×、至近壁での発射禁止、手元追従とトレイル、HP/「燃焼中」の日本語、各弾/着弾破片/消滅を目視。
5. 壁・坂・天井、高速移動、30/60/120 fpsで操作感を確認。Editor終了/パッケージの終了にクラッシュがないこと。

実入力・描画・操作感を確認してからDraft解除とmainへのマージを判断する。
新機能、Combat Playground制作、ネットワーク同期、部位反応は今回の検証範囲外。
