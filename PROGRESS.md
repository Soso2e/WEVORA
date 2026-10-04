# WEVORA Progress

最終更新: 2026-10-05

現在フェーズ: 最小の魔法戦闘ループ（実装済み・未確認、Editorビルド/自動確認済み・ユーザーのPIE確認待ち）

## 現在の状態

- 実装済み・未確認: 既存OnSpellCast/Contextを購読するSpellCastComponentとFire/Wind共通SpellProjectileを追加。認識→Cast→カメラで狙った方向へ発射→実sweep衝突→ApplyPointDamage→既存Enemy HealthのHP減少を接続。飛行処理/入力/Cancelは維持。固定魔法名のクラスやLevel Blueprintロジックは追加しない。
- 実装済み・未確認: 初期マップ`/Game/ThirdPerson/Lvl_ThirdPerson`へ既存`BP_WEVORAEnemy`を1体配置・保存。ラベル`WEVORA_CombatEnemy`、座標`(1100, 0, 452)`、HP100・ダメージ時の仮HP表示を有効化。地形を維持し、Map/外部Actorを再読込して保存を確認。配置スクリプトは既存敵を上書きしない。
- 完了: UE 5.8 Editorビルド成功。最終Headless自動テスト11/11成功（既存9件＋戦闘2件）、テスト警告0・失敗0。保存済みCharacter/Enemy BP、空中ホバー中Fire/Windの実認識/Cast/移動/命中/HP減少、カメラ方向、自己衝突除外、Cancel、近接壁Spawn防止、壁遮断、寿命、終了時の片付けを確認。
- 未確認: PIEの実キーボード/Viewport入力、動きながらの狙いやすさ、操作感、球体/属性ライトの視認性、難易度、Playerビルド。人間側の確認手順は`Source/WEVORA/Spell/README.md`。マップ保存の検証とHeadlessの論理検証は、描画/実プレイの証明ではない。
- 制限: 全Gestureは共通の球を1発発射（Thrust速度1.2倍・Circle半径1.5倍）。Energyによる強さ変化/Modifier/魔法マナ消費/Wind特殊効果/正式HUD/VFX/SE/死亡画面/リスポーン/同期は今回対象外。
- 実装済み・未確認: 無消費の勢いあるジャンプ、頂点付近の約1秒浮遊と自然落下、空中WASDのマナ消費ホバー、Space長押し上昇、Ctrl優先の即時降下、強化したShift、Alt減速＋静止ホバー、入力終了/キャンセル/操作解除を実装。旧地面Traceホバーを廃止し、Walking/Fallingの床・衝突・着地を使用。
- 実装済み・未確認: 飛行用ManaComponent、地上回復、変更通知、仮マナ/飛行状態表示を追加。空中回復なし、マナ不足では高度維持と加速を停止。マナと速度・時間・重力はBlueprintで調整可能。
- 完了: UE 5.8 Editorビルド成功。最終Headlessテスト9/9成功（飛行4件、既存魔法4件、敵AI1件）・テスト警告0・失敗0。保存済みキャラクターBlueprint、World tick、実sweep着地、浮遊時間、マナ消費/回復、入力猶予、Ctrl優先、上昇、Shift後の速度維持/上限、Alt減速を確認。
- 未確認: 実キーボード/Viewport入力、PIEでの飛行操作感、カメラ、アニメーション、天井/壁/坂での体験、30/60/120 fpsの体感、マナ/難易度調整、Playerビルド。ユーザーの指示に従い実際の検証はユーザーが行う。手順は`Source/WEVORA/Movement/README.md`。
- 制限: 飛行用マナの魔法Cast消費への接続、正式HUD、マルチプレイヤー同期は未実装。
- 完了: `BP_WEVORAEnemy` / `BP_WEVORAEnemyProjectile`を`Content/WEVORA/AI`へ作成・保存・コンパイル。浮遊追尾、距離/視線による検出、予備動作、照準射撃、クールダウン、弾の壁衝突・寿命を実装。
- 完了: 再利用可能なHealthComponentを敵とWEVORACharacterへ組込。標準UnrealダメージによるHP減少、被弾時の仮HP表示、HP0の標的除外、敵の消滅、Blueprintイベントを追加。
- 完了: UE 5.8 Editorビルド成功。Headless自動テスト4件（敵AIと既存魔法3件）成功。敵AIは保存したBlueprintの読込・実移動・検出・照準・射撃間隔・実sweep衝突によるHP減少・壁遮断・検出範囲・死亡・購読解除を確認。弾は視線/カメラのTraceを妨げない設定。
- 未確認: 敵BlueprintのPIEでの描画・追尾・回避・難易度、Playerビルド。開いているEditorは作業保存後に再起動して新クラスを読み込むこと。
- 制限: 敵は直進追尾で経路探索なし。シングルプレイヤー向け。死亡画面/リスポーン/ネットワーク同期は未実装。魔法から敵へのダメージは共通Projectileで接続済み。
- 完了: 魔法編みの既存状態・属性・Cast処理を調査。選択/状態通知と表示専用コンポーネントを追加。
- 完了: Characterへ自動組込。Fire/Windの色付き仮ライト、属性別Niagara設定、Idleの短時間プレビュー、編み中の継続表示、Cast/キャンセル/EndPlay時の停止を実装。
- 完了: UE 5.8 WEVORAEditor Win64 Developmentビルド成功。
- 完了: Headless（NullRHI）自動テスト3件（Gestures / StateFlow / SelectionEffects）成功。素材なしの属性色、Idleタイマー、編み中継続表示、Cast/キャンセル、欠落プロフィール、EndPlay購読解除を確認。git diff --check成功。
- 作業中: なし。最小戦闘ループ、飛行・敵の体験確認と魔法の視覚調整はユーザーのPIE確認待ち。
- 未確認: PIEでの入力と見た目、ライト位置・明るさ、実Niagara素材でのパラメータ反映、Playerビルド。
- 問題: 最初のビルドで既存DoRecenterViewのController変数が親クラスメンバーを隠すC4458が発生。ViewControllerへ改名後ビルド成功。
- 問題: 自動テストの初回・2回目はEndPlay購読解除の検証が失敗。テスト用WorldのActor初期化が不足していたため、初期化と終了のライフサイクルを明示して再実行し、3回目は全件成功。3回目のWorld context警告もテスト側の不要なActor Destroyを除去して解消し、最終4回目は3件成功・テスト警告0・失敗0。現在の自動確認に未解決の失敗なし。
- TODO: Fire/Wind用のNiagara素材を制作・設定し、PIEで確認。
- 完了: 属性切替の瞬間は体側の短時間演出、編み中は右手hand_rに追従する表示へ分離。編み中のQでも体側の演出と右手側の属性更新を同時に実行。BodyOffset/HandOffset/HandSocketNameと属性別SelectionSystemで個別調整可能。
- 完了: Editorをユーザーが保存・終了後、UE 5.8 Editorビルド成功。Headless魔法テスト4/4成功・テスト警告0。実BP_ThirdPersonCharacterのhand_r存在、取り付け先、手のWorld位置との一致、旧固定オフセットの置換、体側との独立表示を確認。
- 未確認: 右手アニメーション中の視覚的な追従、体側の高さ・明るさ、素材設定後のPIE表示。発射・着弾VFXは引き続き未実装。
- 次にやること: ユーザーがEditorを開き直し、`Lvl_ThirdPerson`でSpace→空中Altホバー→LMB＋E＋横マウス→E解放→画面中央へ敵を狙い直す→LMB解放。球の命中と敵HP100→75を確認。QでWindも確認。感触の判断後に必要な値だけ調整。

## 確認方法

- 最小戦闘ループ/既存マップ/実入力手順: `Source/WEVORA/Spell/README.md`。結果:`Saved/Automation/SpellCombat/index.json`、ログ:`Saved/Logs/SpellCombatAutomation.log`、ビルド:`Saved/Logs/SpellCombatBuild.log`、配置:`Saved/Logs/CombatMapPlacement.log`。
- 飛行操作/調整/PIEチェック: `Source/WEVORA/Movement/README.md`。最終自動テスト結果: `Saved/Automation/FlightFinal/index.json`、ログ: `Saved/Logs/FlightAutomationFinal.log`。公開入力APIによるHeadless確認であり、実入力・描画・操作感の証明ではない。
- 敵の配置/設定/確認手順: `Source/WEVORA/AI/README.md`。
- 敵を含む自動テスト: UnrealEditor-Cmd.exe <WEVORA.uproject> -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests WEVORA." -TestExit="Automation Test Queue Empty"。
- 敵テスト結果: `Saved/Automation/EnemyAI/index.json`、ログ: `Saved/Logs/EnemyAutomation.log`。
- ビルド: UE_5.8/Engine/Build/BatchFiles/Build.bat WEVORAEditor Win64 Development -Project=<WEVORA.uprojectの絶対パス> -WaitMutex -NoHotReloadFromIDE。
- 自動テスト: UnrealEditor-Cmd.exe <WEVORA.uproject> -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests WEVORA.Spell" -TestExit="Automation Test Queue Empty"。
- テスト結果: `Saved/Automation/SpellSelection/index.json`、ログ: `Saved/Logs/SpellSelectionAutomation.log`。
- PIE: IdleでQを押して属性色が短時間表示され消える。LMB中は表示が継続し、Qで切替。Eで形を作り直しても表示は継続。Cast・途中キャンセルで消える。
- 繰り返し操作、Possess解除、PIE終了で旧表示が残らないことを確認。
- Niagara設定後: READMEの4つのUser Parameterを素材側へ接続し、属性/形/状態の反映を確認。
- WASD / Space / Ctrl / Shift / Altの既存移動、視点リセットもPIEで確認。

## 設計メモ

- 最小戦闘のブランチ`fix/minimal-spell-combat`（直前の飛行実装から継続）。`FWEVORASpellContext`は入力空間の構成データとして維持。`FWEVORASpellLaunch`にWorld Directionと解決済み速度/Damage/Radius/Lifetime/Colorを保持。ElementParametersとResolveSpellが挙動決定、Projectileが実移動/Hit、既存HealthがDamageを担当。今後のEnergy/Modifierはこの境界を拡張し、完成魔法名クラスは増やさない。
- 照準はカメラ中央からWorldStatic/WorldDynamic/PawnへTrace、プレイヤー視点位置からその点へ発射。発射区間をSphere sweepして近い壁の通り抜けを防止。飛行時の体の向きとは独立。編む操作は既存仕様のままカメラも動くため、E解放後/LMB解放前に狙い直す。狙いやすさは人間側で判断。
- 飛行改善のブランチ作成・開発後コミットをユーザーが指示（2026-10-05）。ブランチ`fix/inertial-mana-flight`。Push/PRは実施しない。実際の検証はユーザーが担当。
- 飛行の力と入力は専用CharacterMovementComponentから物理処理前に更新する。標準MovementはActor Tickより先に動くため、Actor Tickで力を変更すると1フレーム遅れる。Actor Tickはカメラ/表示のみ。マルチプレイヤー予測・同期は今回対象外。
- 初期値: ジャンプ900 cm/s、浮遊1秒＋重力復帰0.35秒、入力猶予0.2秒、通常重力1.2、Ctrl重力2.4。マナ最大100・ホバー8/秒・上昇18/秒・Shift12/回・地上回復20/秒（消費後0.75秒待機）。Shift加算1800 cm/s・上限3200 cm/s・クールダウン0.45秒。体験確認後に調整する。
- Git運用: ユーザーより必要な区切りでのコミットを許可（2026-10-04）。関連変更だけを対象とする。Push/PRは別途指示に従う。
- 認識・Cast処理と表示を分離し、OnSpellPresentationChangedで状態とContextを通知。表示のための毎フレーム監視は行わない。
- ElementEffectsは属性Enumをキーとする設定。System未指定時は仮ライトのみ、プロフィール未指定時は旧表示を停止。
- Niagaraコンポーネントと仮ライトは使い回し、EndPlayで解放。専用サーバーには生成しない。
- 魔法のNiagara素材、正式な発射/着弾演出、ネットワーク同期は未実装。魔法Projectile/敵の射撃ダメージとも既存HealthComponentへ接続済み。
- プロジェクト内のAGENTS.md/Concept.md/既存PROGRESS.mdは見つからず。Spell/README.mdを参照。BRAINの20_Projects/30_Decisionsの対象語検索では関連ノートなし。

## 変更履歴

| ID | 内容 | 結果 | 変更ファイル | 次 |
|---|---|---|---|---|
| 2026-10-04-01 | 魔法選択エフェクト基盤、変更通知、Niagara依存、仮ライト、設定手順、ライフサイクルテストを追加。既存C4458を変数改名で修正。Editorビルド成功。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellSelectionEffectComponent.h/.cpp`, `Source/WEVORA/Spell/WEVORASpellWeavingComponent.h/.cpp`, `Source/WEVORA/Spell/README.md`, `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/WEVORA.Build.cs`, `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `WEVORA.uproject`, `PROGRESS.md` | 自動テスト結果記録、Niagara素材設定、PIE確認 |
| 2026-10-04-02 | テスト用Actorの初期化・終了手順を修正。Editor再ビルド成功、Headless自動テスト3/3成功、diffチェック成功。描画・実入力は対象外。 | 実装済み・未確認 | `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `PROGRESS.md` | Niagara素材設定、PIE確認、Playerビルド |
| 2026-10-04-03 | コミット・Push依頼に伴い変更範囲、現在ブランチfeature/spell-weaving-prototype、origin、diffチェックを確認。今回の実装・テスト・文書11ファイルをコミット対象とした。 | 調査完了 | 今回の関連11ファイル | コミット・Push、リモートHEAD一致確認 |
| 2026-10-04-04 | 浮遊敵と弾のC++/配置用Blueprint、HealthComponent、生成スクリプト、配置手順、自動テストを追加。Editorビルド成功・Headless 4/4成功。初回のdebug表示キー型のビルドエラーを修正。初回AIテストのGameMode不在によるPlayerState/被弾初期化不足をテストWorldの初期化で修正。BRAINの対象プロジェクト名ファイル検索で関連ノートなし。 | 実装済み・未確認 | `Source/WEVORA/AI/*`, `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/Tests/WEVORAEnemyTests.cpp`, `Content/WEVORA/AI/*.uasset`, `Scripts/create_enemy_blueprints.py`, `PROGRESS.md` | Editor再起動、配置とPIE確認、Playerビルド |
| 2026-10-04-05 | 選択エフェクトの位置報告を受けC++の取り付け先と初期オフセットを確認。Mesh相対の固定位置で手のソケットへは未接続。希望位置を確認中、位置のコード変更は未実施。 | 調査完了 | `PROGRESS.md` | 希望位置の回答、Blueprint実設定とPIE位置の確認、必要な修正 |
| 2026-10-04-05 | 必要な区切りでのコミット許可を記録。敵AIの関連14ファイルをコミット対象として確認。前回Editorビルド成功・Headless 4/4成功の記録を確認。 | 調査完了 | 敵AIの関連14ファイル | コミット、PIE確認 |
| 2026-10-04-06 | 選択切替は体、編み中は右手に表示する構成へ分離。属性別SelectionSystem、BodyOffset/HandOffset/HandSocketNameを追加。旧Blueprintの固定位置もBeginPlayで右手相対へ置換。初回は開いたEditorのDLLロックでLNK1104、ユーザーが保存・終了後の再ビルド成功。実キャラクターの右手取り付けを含むHeadless魔法4/4成功、テスト警告0。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellSelectionEffectComponent.h/.cpp`, `Source/WEVORA/WEVORACharacter.cpp`, `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `Source/WEVORA/Spell/README.md`, `PROGRESS.md` | Editorを開き直しPIEで体の切替演出と右手追従を確認 |
| 2026-10-04-07 | ユーザーのコミット・Push依頼に伴い、体側の切替演出と右手追従の関連6ファイル、現在ブランチとorigin、diffチェックを確認。直前のEditorビルド成功・魔法テスト4/4成功を引き継ぐ。 | 調査完了 | 今回の関連6ファイル | コミット・Push、リモートHEAD一致確認、PIE確認 |
| 2026-10-05-01 | 飛行改善の操作案を現行Characterの入力・重力・ホバー・加速と照合。無入力時の自然落下、入力によるマナ消費ホバー、静止詠唱時の操作を検討。BRAINのWEVORA対象検索では関連ノートなし。コード変更・ビルド・PIE確認は未実施。 | 調査完了 | `PROGRESS.md` | 静止ホバーとマナ仕様を整理し、実装後PIEで操作感を確認 |
| 2026-10-05-02 | `fix/inertial-mana-flight`を作成。慣性ジャンプ/浮遊/自然落下、マナホバー/上昇、Ctrl優先、Shift強化、Alt静止ホバー、マナ地上回復、入力解除、調整・ユーザー確認手順を追加。初回テストビルドのfloat/double曖昧オーバーロードを修正。初回HeadlessはテストControllerのローカル判定不足で飛行3件失敗、テスト環境を明示ローカル化・EndPlayを追加して飛行4/4成功。最終Editorビルド成功・全9/9成功・テスト警告0。関連9ファイルをコミット対象とし、PIE/実機検証はユーザー担当。 | 実装済み・未確認 | `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/Movement/*`, `Source/WEVORA/Tests/WEVORAFlightTests.cpp`, `PROGRESS.md` | ユーザーのPIE確認と値調整 |
| 2026-10-05-03 | 共通SpellCastComponent/Projectile/LaunchでFire/Wind認識→空中Cast→実sweep→既存Enemy HP減少を接続。既存最初のマップへ敵1体を保存・再読込確認。仮球/色付きライト、Actor名付き仮HP、操作手順を追加。初回テストビルドのTObjectPtr引数を修正。配置Pythonのbool名を修正。初回戦闘テストはHP減少を確認したがテスト環境にLocalPlayerがなくCameraManager更新が止まり方向/壁確認が失敗、テスト側のカメラ計算を明示して解消。最終Editorビルド成功、Headless全11/11成功・テスト警告0。初回の環境音声警告は今回対象外のため最終は-nosoundで実行。実プレイ/Playerは未確認。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellCastComponent.*`, `WEVORASpellProjectile.*`, `WEVORASpellTypes.h`, `Source/WEVORA/WEVORACharacter.*`, `AI/WEVORAHealthComponent.cpp`, `Tests/WEVORASpellCombatTests.cpp`, `Spell/README.md`, `AI/README.md`, `Scripts/place_combat_enemy.py`, `Content/ThirdPerson/Lvl_ThirdPerson.umap`, マップ外部Actor, `PROGRESS.md` | 人間が既存マップで飛行→編み→Cast→命中→HP減少を確認 |
