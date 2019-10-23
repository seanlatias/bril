var arr = [0];
for (let i = 0; i < 4; i = i+1) {
  for (let j = 0; j < 4; j = j+1) {
    let index = i*4 + j;
    arr[index] = i + j;
  }
}
