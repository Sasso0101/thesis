from statistics import geometric_mean


def parse_stdout(stdout: str) -> float:
    lines = stdout.split('\n')
    times = []
    for i in range(2, len(lines), 2):
        times.append(float(lines[i].split(',')[-1]))
    print(times)
    return geometric_mean(times)


sout = '''
Trial time:          4.42019
"BFS",0,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",4.4202
Trial time:          5.67220
"BFS",1,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",5.6722
Trial time:          4.34982
"BFS",2,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",4.3498
Trial time:          6.00608
"BFS",3,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",6.0061
Trial time:          5.71320
"BFS",4,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",5.7132
Trial time:          5.50007
"BFS",5,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",5.5001
Trial time:          5.46055
"BFS",6,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",5.4606
Trial time:          5.16595
"BFS",7,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",5.1660
Trial time:          5.54589
"BFS",8,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",5.5459
Trial time:          3.74587
"BFS",9,"dataset=datasets/large_diameter/GAP/GAP-road/GAP-road.bmtx,threads=24,chunk_size=64",3.7459
'''

print(parse_stdout(sout))