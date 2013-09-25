#!/bin/bash

function compressify()
{
  dir=$1

  cd $dir
  for model in `find . -not -name '*.gz' -not -name '.'`
  do
    filename=$(basename "$model")
    ext="${filename##*.}"
    name="${filename%.*}"


    # set output extension to 'osgt' as we need to be able to serialize user data
    ext="osgt"

    osgconv $model ${name}_vtx_q1.$ext.qtz -O "vertex quantization=1"
    osgconv $model ${name}_vtx_q2.$ext.qtz -O "vertex quantization=2"
    osgconv $model ${name}_vtx_q3.$ext.qtz -O "vertex quantization=3"

    osgconv $model ${name}_nor_q1.$ext.qtz -O "normal quantization=1"
    osgconv $model ${name}_nor_q2.$ext.qtz -O "normal quantization=2"
    osgconv $model ${name}_nor_q3.$ext.qtz -O "normal quantization=3"

    osgconv $model ${name}_vtx_p.$ext.qtz -O "vertex prediction"
    osgconv $model ${name}_vtx_p.$ext.qtz -O "vertex prediction"
    osgconv $model ${name}_vtx_p.$ext.qtz -O "vertex prediction"

    osgconv $model ${name}_nor_p.$ext.qtz -O "normal prediction"
    osgconv $model ${name}_nor_p.$ext.qtz -O "normal prediction"
    osgconv $model ${name}_nor_p.$ext.qtz -O "normal prediction"

    osgconv $model ${name}_vtx_p_q1.$ext.qtz -O "vertex prediction quantization=1"
    osgconv $model ${name}_vtx_p_q2.$ext.qtz -O "vertex prediction quantization=2"
    osgconv $model ${name}_vtx_p_q3.$ext.qtz -O "vertex prediction quantization=3"

    osgconv $model ${name}_nor_p_q1.$ext.qtz -O "normal prediction quantization=1"
    osgconv $model ${name}_nor_p_q2.$ext.qtz -O "normal prediction quantization=2"
    osgconv $model ${name}_nor_p_q3.$ext.qtz -O "normal prediction quantization=3"

    osgconv $model ${name}_vtx_nor_q1.$ext.qtz -O "vertex normal quantization=1"
    osgconv $model ${name}_vtx_nor_q2.$ext.qtz -O "vertex normal quantization=2"
    osgconv $model ${name}_vtx_nor_q3.$ext.qtz -O "vertex normal quantization=3"

    osgconv $model ${name}_vtx_nor_p.$ext.qtz -O "vertex normal prediction"
    osgconv $model ${name}_vtx_nor_p.$ext.qtz -O "vertex normal prediction"
    osgconv $model ${name}_vtx_nor_p.$ext.qtz -O "vertex normal prediction"

    osgconv $model ${name}_vtx_nor_p_q1.$ext.qtz -O "vertex normal prediction quantization=1"
    osgconv $model ${name}_vtx_nor_p_q2.$ext.qtz -O "vertex normal prediction quantization=2"
    osgconv $model ${name}_vtx_nor_p_q3.$ext.qtz -O "vertex normal prediction quantization=3"

    for compressed in `ls ${name}*.qtz`
    do
        gzip -c $compressed > ${compressed}.gz
    done
  done
  cd -
}
