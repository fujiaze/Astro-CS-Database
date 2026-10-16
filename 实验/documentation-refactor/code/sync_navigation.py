"""Synchronize path-only metadata and the current docs navigation index."""
import json
from pathlib import Path
import re
import subprocess
import sys
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))
from restructure import ROOT, tracked, translate
BASELINE = '63eccbb6133aef56389641d43c55df6e243819c5'


def main():
    mapping = json.loads((ROOT / '实验/documentation-refactor/docs/PATH_MIGRATIONS.json').read_text(encoding='utf-8'))
    metadata_record = ROOT / '实验/documentation-refactor/docs/METADATA_PATH_UPDATES.json'
    changed_metadata = json.loads(metadata_record.read_text(encoding='utf-8')) if metadata_record.exists() else []
    for name in tracked():
        if not name.startswith(('eng/contracts/', 'eng/packaging/')) or not name.endswith('.json'):
            continue
        path = ROOT / name
        text = path.read_text(encoding='utf-8-sig')
        updated = text
        for old, (new, _) in mapping.items():
            # Retired line locations must not be transferred to a different page.
            updated = re.sub(re.escape(old) + r':\d+(?:[-–]\d+)?', new, updated)
            updated = updated.replace(old, new)
        if updated != text:
            json.loads(updated)  # This operation must preserve valid JSON.
            path.write_text(updated, encoding='utf-8', newline='\n')
            if name not in changed_metadata:
                changed_metadata.append(name)
    index = ROOT / 'docs/DOCUMENT_INDEX.yaml'
    # Rebuild from the baseline so consolidation does not preserve duplicate paths.
    baseline = subprocess.check_output(['git', 'show', BASELINE + ':docs/DOCUMENT_INDEX.yaml'], cwd=ROOT).decode('utf-8')
    data = yaml.safe_load(baseline)
    entries = []
    for row in data['doc_index']['active']:
        if row['path'] in mapping:
            continue
        row = dict(row)
        for old, (new, _) in mapping.items():
            row['upstream'] = row['upstream'].replace(old, new)
        if row['path'] == 'docs/DOCUMENT_INDEX.yaml':
            row['status'] = 'ACTIVE_INFORMATIVE'
            row['duty'] = '当前 docs 文件的职责与阅读导航，不定义设计或验证状态'
        if row['path'] == 'docs/engineering/governance/DOCUMENT_GOVERNANCE.md':
            row['duty'] = '文档归属、写法、协作分工、追溯、索引和证据维护'
        if row['path'] == 'docs/engineering/api/PUBLIC_API.md':
            row['duty'] = '公共接口分层、语义、所有权、错误和并发规则；实际头及模块正文导航'
        if row['path'] == 'docs/engineering/contracts/RUNTIME.md':
            row['duty'] = 'Runtime、registry、模块、RunContext 与线程租约的实际接口和目标边界'
        if row['path'] == 'docs/engineering/contracts/OWNERSHIP_LIFETIME.md':
            row['duty'] = '调用方与库分配、句柄释放、异步借用、取消及消费者寿命'
        if row['path'] == 'docs/detail/registry/README.md':
            row['duty'] = '模块说明的阶段导航，接口正文位于各阶段 modules 与共用 writer'
        entries.append(row)
    for stage in ['normalize', 'mosaic', 'export']:
        entries.append(dict(
            path=f'docs/detail/{stage}/modules.md', status='ACTIVE_INFORMATIVE',
            duty=f'{stage} 模块的输入输出、公开接口、所有权、错误与生产调用说明',
            upstream=f'docs/detail/{stage}/pipeline.md'))
    entries.append(dict(path='docs/detail/infrastructure/hips_writer.md',
                        status='ACTIVE_INFORMATIVE',
                        duty='共用 HiPS writer 的视图、单位、句柄、错误与层级写出',
                        upstream='docs/engineering/contracts/HIPS_IO.md'))
    for name, duty, upstream in [
        ('platesolve', 'IPV完整C接口、参数/结果/回调布局、选星交接、错误和生命周期',
         'docs/science/algorithms/normalize/PLATESOLVE.md'),
    ]:
        entries.append(dict(path=f'docs/detail/normalize/modules/{name}.md',
                            status='ACTIVE_INFORMATIVE', duty=duty,
                            upstream=upstream))
    data['doc_index']['active'] = sorted(entries, key=lambda x: x['path'])
    header = ('# ACSD 文档导航；path 相对仓库根且唯一。\n'
              '# 用途分类不表示构建、真实运行或科学结论已验证。\n'
              '# science 与 engineering 同级，detail 说明实际接口与实施。\n')
    index.write_text(header + yaml.safe_dump(data, allow_unicode=True, sort_keys=False,
                                            width=120), encoding='utf-8', newline='\n')
    # The fixture contains historical line evidence, not live navigation.
    name = 'eng/tools/doccheck/assert_dup_fixtures.yaml'
    (ROOT / name).write_bytes(subprocess.check_output(['git', 'show', BASELINE + ':' + name], cwd=ROOT))
    (ROOT / '实验/documentation-refactor/docs/METADATA_PATH_UPDATES.json').write_text(
        json.dumps(changed_metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Indexed {len(entries)} current docs; updated navigation strings in {len(changed_metadata)} JSON files.')


if __name__ == '__main__':
    main()
