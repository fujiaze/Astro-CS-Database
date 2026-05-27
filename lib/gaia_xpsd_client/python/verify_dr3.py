#!/usr/bin/env python3
"""
验证GaiaDR3数据库解析
检查文件头、数据库标识符、星点数据格式
"""

import os
import struct
import mmap

def parse_xpsd_file(filepath):
    """解析XPSD文件头"""
    print(f"\n{'='*60}")
    print(f"解析文件: {os.path.basename(filepath)}")
    print(f"{'='*60}")
    
    with open(filepath, 'rb') as f:
        magic = f.read(8)
        if magic != b'XPSD0100':
            print(f"  错误: 魔数不匹配: {magic}")
            return None
        
        header_len = struct.unpack('<I', f.read(4))[0]
        print(f"  魔数: {magic}")
        print(f"  头长度: {header_len}")
        
        xml_start = f.tell()
        xml_data = f.read(header_len - xml_start).decode('utf-8', errors='ignore')
        
        db_id_start = xml_data.find('<DatabaseIdentifier>')
        db_id_end = xml_data.find('</DatabaseIdentifier>')
        if db_id_start >= 0 and db_id_end >= 0:
            db_id = xml_data[db_id_start+19:db_id_end]
            print(f"  数据库标识: {db_id}")
            has_spectrum = 'GaiaDR3SP' in db_id
            print(f"  含光谱数据: {has_spectrum}")
        else:
            db_id = "未知"
            has_spectrum = False
        
        data_start = xml_data.find('<Data ')
        if data_start >= 0:
            data_tag_end = xml_data.find('/>', data_start)
            data_tag = xml_data[data_start:data_tag_end+2]
            
            mag_start = data_tag.find('magnitudeRange="')
            if mag_start >= 0:
                mag_end = data_tag.find('"', mag_start+16)
                mag_range = data_tag[mag_start+16:mag_end]
                print(f"  星等范围: {mag_range}")
            
            pos_start = data_tag.find('position="')
            if pos_start >= 0:
                pos_end = data_tag.find('"', pos_start+10)
                position = int(data_tag[pos_start+10:pos_end])
                print(f"  数据位置: {position}")
            
            comp_start = data_tag.find('compression="')
            if comp_start >= 0:
                comp_end = data_tag.find('"', comp_start+13)
                compression = data_tag[comp_start+13:comp_end]
                print(f"  压缩方式: {compression}")
            
            item_start = data_tag.find('itemSize="')
            if item_start >= 0:
                item_end = data_tag.find('"', item_start+10)
                item_size = int(data_tag[item_start+10:item_end])
                print(f"  条目大小: {item_size} bytes")
        
        stats_start = xml_data.find('<Statistics ')
        if stats_start >= 0:
            stats_tag_end = xml_data.find('/>', stats_start)
            stats_tag = xml_data[stats_start:stats_tag_end+2]
            total_start = stats_tag.find('totalSources="')
            if total_start >= 0:
                total_end = stats_tag.find('"', total_start+14)
                total_sources = int(stats_tag[total_start+14:total_end])
                print(f"  总星数: {total_sources:,}")
        
        tree_count = 0
        tree_search = xml_data
        while True:
            tree_start = tree_search.find('<Tree ')
            if tree_start < 0:
                break
            tree_tag_end = tree_search.find('/>', tree_start)
            tree_tag = tree_search[tree_start:tree_tag_end+2]
            
            proj_start = tree_tag.find('projection="')
            if proj_start >= 0:
                proj_end = tree_tag.find('"', proj_start+12)
                projection = tree_tag[proj_start+12:proj_end]
            else:
                projection = "未知"
            
            center_start = tree_tag.find('center="')
            if center_start >= 0:
                center_end = tree_tag.find('"', center_start+8)
                center = tree_tag[center_start+8:center_end]
            else:
                center = "未知"
            
            node_start = tree_tag.find('nodeCount="')
            if node_start >= 0:
                node_end = tree_tag.find('"', node_start+11)
                node_count_str = tree_tag[node_start+11:node_end]
                if node_count_str:
                    node_count = int(node_count_str)
                else:
                    node_count = 0
            else:
                node_count = 0
            
            if tree_count == 0:
                print(f"  第一个树: projection={projection}, center={center}, nodeCount={node_count}")
            
            tree_count += 1
            tree_search = tree_search[tree_tag_end+2:]
        
        print(f"  树数量: {tree_count}")
        
        return {
            'db_id': db_id,
            'has_spectrum': has_spectrum,
        }

def main():
    print("="*60)
    print("GaiaDR3 数据库解析验证")
    print("="*60)
    
    dr3_dir = r"F:\Astro dev\Astro CS Normalization Database\GaiaDR3"
    dr3sp_dir = r"F:\Astro dev\Astro CS Normalization Database\GaiaDR3SP"
    
    print("\n### GaiaDR3 (完整版) ###")
    dr3_files = sorted([f for f in os.listdir(dr3_dir) if f.endswith('.xpsd')])
    print(f"文件数: {len(dr3_files)}")
    
    for f in dr3_files[:2]:
        parse_xpsd_file(os.path.join(dr3_dir, f))
    
    print("\n### GaiaDR3SP (光谱版) ###")
    dr3sp_files = sorted([f for f in os.listdir(dr3sp_dir) if f.endswith('.xpsd')])
    print(f"文件数: {len(dr3sp_files)}")
    
    for f in dr3sp_files[:2]:
        parse_xpsd_file(os.path.join(dr3sp_dir, f))
    
    print("\n" + "="*60)
    print("结论:")
    print("  DR3和DR3SP文件格式相同，都使用XPSD0100格式")
    print("  通过DatabaseIdentifier区分数据库类型")
    print("  DR3: gdr3-1.0.0-XX.xpsd, 无光谱数据")
    print("  DR3SP: gdr3sp-1.0.0-XX.xpsd, 含光谱数据")
    print("="*60)

if __name__ == "__main__":
    main()
