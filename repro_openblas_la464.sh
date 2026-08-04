#!/usr/bin/env bash
# =============================================================================
# 复现 OpenBLAS v0.3.34 LA464 dgemm_kernel_16x6.S:423 SIGSEGV
#
# 崩溃现场:
#   Testing DOUBLE PRECISION Linear-Equation-routines-LIN/xlintstd < dtest.in
#   SIGSEGV at ../kernel/loongarch64/dgemm_kernel_16x6.S:423  (xvld U0, C0, 0x00)
#
# 触发命令(用户原始):
#   make -C openmp TARGET=LA464 DYNAMIC_ARCH=1 USE_THREAD=1 USE_OPENMP=1 \
#        FC=gfortran CC=gcc \
#        'COMMON_OPT=-O2 -g -fPIC -fopenmp -pthread' \
#        'FCOMMON_OPT=-O2 -g -fPIC -fopenmp -pthread -frecursive' \
#        NUM_THREADS=128 LIBPREFIX=libopenblaso INTERFACE64=0 \
#        CPP_THREAD_SAFETY_TEST=1:1 lapack-test
#
# 用法:
#   ./repro_openblas_la464.sh            # 全量构建 + 跑 dtest + gdb 抓现场
#   ./repro_openblas_la464.sh --bisect   # 额外跑 5 组二分矩阵定位触发开关
#   ./repro_openblas_la464.sh --gdb-only # 跳过构建, 直接对已有产物跑 gdb
#
# 环境变量(可选):
#   OPENBLAS_SRC   指向已解压的 openblas-0.3.34 源码树; 不设则 git clone
#   BUILDDIR       构建根目录(默认 ./repro-work)
# =============================================================================
set -uo pipefail

# ----------------------------- 配置 ------------------------------------------
OPENBLAS_VER="v0.3.34"
OPENBLAS_REPO="https://github.com/OpenMathLib/OpenBLAS.git"
BUILDDIR="${BUILDDIR:-$(pwd)/repro-work}"
CC="${CC:-gcc}"
FC="${FC:-gfortran}"
COMMON_OPT="-O2 -g -fPIC -fopenmp -pthread"
FCOMMON_OPT="-O2 -g -fPIC -fopenmp -pthread -frecursive"

# 颜色
C_RED='\033[0;31m'; C_GRN='\033[0;32m'; C_YEL='\033[1;33m'; C_BLU='\033[0;34m'; C_N='\033[0m'
log()  { printf "${C_BLU}[*]${C_N} %s\n" "$*"; }
ok()   { printf "${C_GRN}[+]${C_N} %s\n" "$*"; }
warn() { printf "${C_YEL}[!]${C_N} %s\n" "$*"; }
err()  { printf "${C_RED}[-]${C_N} %s\n" "$*"; }

# ----------------------------- 环境检查 --------------------------------------
check_env() {
  log "环境检查"
  local arch; arch=$(uname -m)
  if [[ "$arch" != "loongarch64" ]]; then
    warn "当前架构 '$arch', 本脚本专为 loongarch64 设计; 继续可能导致结果不可信"
  fi
  for t in "$CC" "$FC" gdb make git; do
    if ! command -v "$t" >/dev/null 2>&1; then
      err "缺少工具: $t"; exit 1
    fi
  done
  ok "工具链就绪: CC=$CC FC=$FC"
}

# ----------------------------- 源码准备 --------------------------------------
prepare_source() {
  if [[ -n "${OPENBLAS_SRC:-}" && -f "$OPENBLAS_SRC/Makefile" ]]; then
    log "使用已有源码: $OPENBLAS_SRC"
    SRC_DIR="$OPENBLAS_SRC"
    return
  fi
  SRC_DIR="$BUILDDIR/OpenBLAS"
  if [[ -d "$SRC_DIR/.git" ]]; then
    log "源码已存在, 跳过 clone: $SRC_DIR"
  else
    log "clone OpenBLAS $OPENBLAS_VER -> $SRC_DIR"
    mkdir -p "$BUILDDIR"
    git clone --depth 1 --branch "$OPENBLAS_VER" "$OPENBLAS_REPO" "$SRC_DIR" || {
      err "git clone 失败"; exit 1
    }
  fi
}

# ----------------------------- 构建 ------------------------------------------
# 用法: build_combo "label" "FLAG1=.. FLAG2=.."
build_combo() {
  local label="$1" extra="$2"
  local bdir="$BUILDDIR/build-$label"
  log "[$label] 清理并构建: $extra"
  rm -rf "$bdir"; mkdir -p "$bdir"
  # OpenBLAS out-of-tree: 进构建目录, 用 -f 指向源码 Makefile
  local build_log="$BUILDDIR/build-$label.log"
  ( cd "$bdir" && \
    make -f "$SRC_DIR/Makefile" \
        TARGET=LA464 USE_THREAD=1 USE_OPENMP=1 \
        CC="$CC" FC="$FC" \
        "COMMON_OPT=$COMMON_OPT" \
        "FCOMMON_OPT=$FCOMMON_OPT" \
        LIBPREFIX=libopenblaso \
        $extra \
        lapack-test ) 2>&1 | tee "$build_log"
  local rc=${PIPESTATUS[0]}
  if [[ $rc -ne 0 ]]; then
    err "[$label] 构建失败 (rc=$rc), 详见 $build_log"
    return 1
  fi
  ok "[$label] 构建完成"
}

# 定位某 build 目录下的 xlintstd 与 dtest.in
find_artifacts() {
  local bdir="$1"
  XLINTSTD=$(find "$bdir" -name xlintstd -type f -perm -u+x 2>/dev/null | head -1)
  DTEST_IN=$(find "$bdir" -path '*/TESTING/dtest.in' 2>/dev/null | head -1)
  if [[ -z "$XLINTSTD" || -z "$DTEST_IN" ]]; then
    err "未找到 xlintstd 或 dtest.in (bdir=$bdir)"
    err "  xlintstd=$XLINTSTD"
    err "  dtest.in =$DTEST_IN"
    return 1
  fi
  ok "产物: xlintstd=$XLINTSTD"
  ok "      dtest.in=$DTEST_IN"
}

# ----------------------------- 跑 dtest --------------------------------------
# 返回: 0=通过, 1=SIGSEGV, 2=其他失败
run_dtest() {
  local label="$1" bdir="$2"
  local out="$BUILDDIR/dtest-$label.out"
  local errf="$BUILDDIR/dtest-$label.err"
  log "[$label] 运行 xlintstd < dtest.in"
  # xlintstd 需要在其所在目录运行(它会读相对路径), dtest.in 单独指定
  local xdir; xdir=$(dirname "$XLINTSTD")
  ( cd "$xdir" && "./xlintstd" < "$DTEST_IN" > "$out" 2> "$errf" )
  local rc=$?
  if [[ $rc -eq 139 ]]; then
    err "[$label] SIGSEGV (rc=139) — 复现成功"
    err "    stdout: $out"
    err "    stderr: $errf"
    return 1
  elif [[ $rc -ne 0 ]]; then
    warn "[$label] 非零退出 rc=$rc (非段错误, 可能是数值错误)"
    warn "    stdout: $out ; stderr: $errf"
    return 2
  else
    ok "[$label] 通过 (rc=0)"
    return 0
  fi
}

# ----------------------------- gdb 抓现场 ------------------------------------
capture_gdb() {
  local label="$1"
  local gdb_log="$BUILDDIR/gdb-$label.log"
  log "[$label] gdb 抓现场 -> $gdb_log"
  # LoongArch LP64 + OpenBLAS 内核入参寄存器约定:
  #   r4=M(bm) r5=N(bn) r6=K(bk) r7=A r8=B r9=C r10=LDC r11=OFFSET r12=OFF
  local xdir; xdir=$(dirname "$XLINTSTD")
  ( cd "$xdir" && gdb -batch -nx \
      -ex 'set pagination off' \
      -ex 'set confirm off' \
      -ex "run < $DTEST_IN > $BUILDDIR/dtest-$label.gdb.out" \
      -ex 'echo \n========== SIGNAL ==========\n' \
      -ex 'info signals SIGSEGV' \
      -ex 'echo \n========== REGISTERS (kernel args) ==========\n' \
      -ex 'echo r4=M  r5=N  r6=K  r7=A  r8=B  r9=C  r10=LDC  r11=OFFSET  r12=OFF\n' \
      -ex 'info registers r4 r5 r6 r7 r8 r9 r10 r11 r12' \
      -ex 'echo \n========== BACKTRACE ==========\n' \
      -ex 'bt' \
      -ex 'echo \n========== FRAME AT dgemm_kernel_16x6.S ==========\n' \
      -ex 'bt full' \
      --args "$XLINTSTD" ) 2>&1 | tee "$gdb_log"

  # 提取关键诊断
  echo
  warn "[$label] 关键诊断摘要:"
  grep -E 'SIGSEGV|Program received signal|dgemm_kernel_16x6\.S|in dgemm_kernel|^\s+r4\b|^\s+r5\b|^\s+r6\b|^\s+r9\b|^\s+r10\b' "$gdb_log" \
    | sed 's/^/    /' | head -40
}

# ----------------------------- 二分矩阵 --------------------------------------
# 每个 entry: "label|额外make标志"
BISECT_COMBOS=(
  # A: 用户原始配置(全开) —— 必崩
  "full|DYNAMIC_ARCH=1 NUM_THREADS=128 INTERFACE64=0 CPP_THREAD_SAFETY_TEST=1:1"
  # B: 去掉 DYNAMIC_ARCH —— 测试是否动态派发导致
  "no-dynarch|NUM_THREADS=128 INTERFACE64=0 CPP_THREAD_SAFETY_TEST=1:1"
  # C: 降低 NUM_THREADS —— 测试是否 128 线程切边越界
  "low-threads|DYNAMIC_ARCH=1 NUM_THREADS=32 INTERFACE64=0 CPP_THREAD_SAFETY_TEST=1:1"
  # D: 去掉 CPP_THREAD_SAFETY_TEST —— 测试该开关是否有副作用
  "no-cpptest|DYNAMIC_ARCH=1 NUM_THREADS=128 INTERFACE64=0"
  # E: 最简配置 —— 应通过, 作为对照
  "minimal|NUM_THREADS=32 INTERFACE64=0"
)

run_bisect() {
  log "开始二分矩阵 (${#BISECT_COMBOS[@]} 组)"
  printf "%-14s | %-10s | %s\n" "Combo" "Result" "Flags"
  printf -- "---------------+------------+----------------\n"
  local summary="$BUILDDIR/bisect-summary.txt"
  : > "$summary"
  for entry in "${BISECT_COMBOS[@]}"; do
    local label="${entry%%|*}"; local flags="${entry#*|}"
    if build_combo "$label" "$flags"; then
      if find_artifacts "$BUILDDIR/build-$label"; then
        if run_dtest "$label" "$BUILDDIR/build-$label"; then
          printf "%-14s | %-10s | %s\n" "$label" "PASS" "$flags"
          echo "$label PASS | $flags" >> "$summary"
        else
          rc=$?
          if [[ $rc -eq 1 ]]; then
            printf "%-14s | %-10s | %s\n" "$label" "SIGSEGV" "$flags"
            echo "$label SIGSEGV | $flags" >> "$summary"
          else
            printf "%-14s | %-10s | %s\n" "$label" "FAIL($rc)" "$flags"
            echo "$label FAIL($rc) | $flags" >> "$summary"
          fi
        fi
      else
        printf "%-14s | %-10s | %s\n" "$label" "NO-ART" "$flags"
        echo "$label NO-ART | $flags" >> "$summary"
      fi
    else
      printf "%-14s | %-10s | %s\n" "$label" "BUILD-FAIL" "$flags"
      echo "$label BUILD-FAIL | $flags" >> "$summary"
    fi
  done
  echo
  ok "二分汇总写入: $summary"
  cat "$summary"
}

# ----------------------------- 主流程 ----------------------------------------
main() {
  local mode="full"
  if [[ $# -ge 1 ]]; then
    case "$1" in
      --bisect)   mode="bisect" ;;
      --gdb-only) mode="gdb-only" ;;
      --help|-h)
        sed -n '2,30p' "$0"; exit 0 ;;
      *) err "未知参数: $1"; exit 1 ;;
    esac
  fi

  check_env
  mkdir -p "$BUILDDIR"
  prepare_source

  if [[ "$mode" == "gdb-only" ]]; then
    # 复用已有 full 构建
    local bdir="$BUILDDIR/build-full"
    [[ -d "$bdir" ]] || { err "build-full 不存在, 先跑全量构建"; exit 1; }
    find_artifacts "$bdir" || exit 1
    capture_gdb "full"
    exit 0
  fi

  # 1. 全量复现
  log "==== 步骤 1: 全量构建 (用户原始配置) ===="
  build_combo "full" "DYNAMIC_ARCH=1 NUM_THREADS=128 INTERFACE64=0 CPP_THREAD_SAFETY_TEST=1:1" || exit 1
  find_artifacts "$BUILDDIR/build-full" || exit 1

  log "==== 步骤 2: 运行 dtest ===="
  if run_dtest "full" "$BUILDDIR/build-full"; then
    warn "全量配置未复现崩溃 — 可能需要更大数据集或更高并发压力"
    warn "可尝试: 多跑几次, 或 export GOTOBLAS_NUM_THREADS=128 后重试"
  else
    rc=$?
    if [[ $rc -eq 1 ]]; then
      ok "已复现 SIGSEGV, 进入 gdb 抓现场"
      log "==== 步骤 3: gdb 抓现场 ===="
      capture_gdb "full"
    fi
  fi

  # 2. 二分(可选)
  if [[ "$mode" == "bisect" ]]; then
    echo
    log "==== 步骤 4: 二分矩阵定位触发开关 ===="
    run_bisect
  fi

  echo
  ok "全部完成. 产物目录: $BUILDDIR"
  log "关键文件:"
  echo "    构建日志:  $BUILDDIR/build-*.log"
  echo "    测试输出:  $BUILDDIR/dtest-*.out / .err"
  echo "    gdb 现场:  $BUILDDIR/gdb-*.log"
  echo "    二分汇总:  $BUILDDIR/bisect-summary.txt"
  echo
  warn "若复现成功, 请把 gdb-*.log 与 bisect-summary.txt 附到上游 issue:"
  warn "    https://github.com/OpenMathLib/OpenBLAS/issues/new"
}

main "$@"
