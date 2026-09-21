# REAL-SCIENCE-001 — 可复现命令清单

基线 HEAD = 5eb702bd72649e498022ca34e4810886b1a6b800；工作目录 = 仓库根。
所有命令均带 timeout；括号内为实测退出码。

## 0. 环境校验
    timeout 30 ./build/astrocs --version                     # 期望 +g5eb702bd…（rc 0）
    timeout 3000 cmake --build build --target astrocs -j8     # 若版本串陈旧则重建（rc 0）

## 1. 真实数据抽取（只读 testdata/）
    timeout 3600 python3 run/v6/real-science/extract_real.py \
        --out run/v6/real-science/inputs --frames 5           # rc 0
    # 产出 run/v6/real-science/inputs/{M42,M42_Halpha,Galaxy_Center,LDN43,NGC1727,NGC247,NGC55,NGC83,Victory_Nebula}.json
    # 日志 run/v6/real-science/logs/01_extract_all.log

## 2. 测量驱动 + 负向 harness
    timeout 900 cmake -S run/v6/real-science/driver -B run/v6/real-science/build
    timeout 900 cmake --build run/v6/real-science/build -j8    # rc 0
    timeout 900 /usr/bin/time -v run/v6/real-science/build/v6_real_science_driver \
        --in run/v6/real-science/inputs \
        --out run/v6/real-science/measurements/five_mode_measurements.json \
        --csv run/v6/real-science/measurements/five_mode_measurements.csv   # rc 0
    timeout 120 run/v6/real-science/build/v6_real_science_negative           # rc 0（40/40）

## 3. CLI 模式门负向检查
    timeout 600 bash run/v6/real-science/oracle/cli_mode_gate_check.sh        # rc 0（15/15）

## 4. 独立 Oracle 与汇总
    timeout 900 python3 run/v6/real-science/oracle/v6_real_science_oracle.py \
        --inputs run/v6/real-science/inputs \
        --measurements run/v6/real-science/measurements/five_mode_measurements.json \
        --out run/v6/real-science/measurements/oracle_results.json            # rc 0（486/486）
    timeout 300 python3 run/v6/real-science/summarize.py                      # rc 0
    timeout 300 python3 run/v6/real-science/make_provenance.py                # rc 0

## 5. 结果落位
    # 测量产物 -> artifacts/v6/real-science/
    # 报告     -> reports/v6/real-science/
    # 日志     -> run/v6/real-science/logs/

## 复现判定
- Oracle：n_checks=486，n_fail=0，all_pass=true
- 负向 harness：summary.pass=40，summary.fail=0
- CLI 模式门：CLI_MODE_GATE fail=0
